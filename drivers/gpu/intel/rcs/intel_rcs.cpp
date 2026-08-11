// intel_rcs.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 05.08.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "intel_rcs.h"
#include "gen9_kernels.h"
#include "gfx_pipeline_regs.h"
#include "gfx_pipeline_stats_regs.h"
#include "blend_state.h"
#include "cmd_3dstate_blend_state_pointers.h"
#include "cmd_state_base_address.h"
#include "cmd_pipe_control.h"
#include "cmd_vertex_elements.h"
#include "cmd_vertex_buffers.h"
#include "cmd_3dstate_ps.h"
#include "cmd_3dstate_ps_blend.h"
#include "cmd_3dstate_vf_topology.h"
#include "cmd_3dstate_vf_statistics.h"
#include "cmd_3dstate_urb.h"
#include "cmd_3dstate_vs.h"
#include "cmd_3dstate_wm.h"
#include "cmd_viewport.h"
#include "cmd_scissor.h"
#include "cmd_render_surface_state.h"
#include "cmd_3dstate_binding_table.h"
#include "cmd_3dstate_depth_buffer.h"
#include "cmd_3dstate_drawing_rectangle.h"
#include "surface_format.h"
#include <vespera/log.h>

#include "cmd_3dprimitive.h"
#include "cmd_3dstate_clip.h"
#include "cmd_3dstate_ds.h"
#include "cmd_3dstate_gs.h"
#include "cmd_3dstate_hs.h"
#include "cmd_3dstate_sbe.h"
#include "cmd_3dstate_sf.h"
#include "cmd_3dstate_raster.h"
#include "cmd_3dstate_multisample.h"
#include "cmd_3dstate_sample_mask.h"
#include "cmd_3dstate_te.h"
#include "cmd_3dstate_wm_depth_stencil.h"
#include "drivers/mmio_post_write.h"
#include "gpu/intel/gt_interrupt_regs.h"
#include "gpu/intel/intel_bcs.h"
#include "klib/string.h"
#include "vespera/mm/memory.h"

struct ShaderOffsets {
    u32 vs_offset;
    u32 ps_offset;
};

ShaderOffsets upload_shaders(u8* inst_base_cpu) {
    ShaderOffsets offsets{};
    offsets.vs_offset = 0x000; // 64-Byte aligned
    offsets.ps_offset = 0x0C0; // 64-Byte aligned

    memcpy(inst_base_cpu + offsets.vs_offset, gen9_vs_kernel, gen9_vs_kernel_size);
    memcpy(inst_base_cpu + offsets.ps_offset, gen9_ps_kernel_simd8, gen9_ps_kernel_simd8_size);

    // Cache-Line-Flush für CPU-Cache (64 Bytes pro Line)
    for (size_t i = 0; i < 0x200; i += 64) {
        asm volatile("clflush (%0)" :: "r"(inst_base_cpu + i) : "memory");
    }
    asm volatile("mfence" ::: "memory");

    return offsets;
}

namespace blt {
    IntelRcs::IntelRcs(IntelGpuDevice& device)
        : IntelEngine(
            EngineType::RCS, device, RCS_ENGINE_OFFSET,
            ForceWakeDomain{
                FORCEWAKE_RENDER, FORCEWAKE_ACK_RENDER, FORCEWAKE_RENDER_ENABLE, FORCEWAKE_ACK_BIT,
                FORCEWAKE_RENDER_TIMEOUT
            }
        ) {
    }

    bool IntelRcs::init_device() {
        if (!engine_force_wake_enable()) {
            Log::info("intel-rcs: ForceWake timeout");
            return false;
        }

        if (!engine_reset()) return false;
        Log::debug("RCS reset successful");

        hwsp_alloc();
        ring_alloc_and_init(RCS_RING_BUFFER_SIZE);

        HWSTAM_REG stam{};
        stam.raw = 0xFFFFFFFFu;
        engine_reg_write(ENGINE_HWSTAM_OFF, stam);

        completion_flag_.init(false);

        rcs_interrupts_enable();

        if (!device().register_engine_for_irq(this)) {
            Log::info("intel-rcs: failed to register for GT interrupts");
            return false;
        }

        Log::info("intel-rcs: ring + HWSP initialized (legacy ring-buffer mode, Milestone 1)");

        // NEXT STEPS (not yet implemented, in order):
        //   3. Vertex buffer alloc + 3DSTATE_VERTEX_BUFFERS/ELEMENTS
        //   4. Minimal VS/PS kernel upload (Instruction State) + 3DSTATE_VS/PS
        //   5. 3DSTATE_VIEWPORT / SCISSOR / DEPTH_BUFFER / DRAWING_RECTANGLE
        //   6. 3DPRIMITIVE (TRILIST, 3 vertices)
        //   7. PIPE_CONTROL w/ Post-Sync seqno write + ring_flush() + seqno_wait()
        // Milestone 1 only proved ForceWake + ring/HWSP allocation. This
        // function now additionally selects the 3D pipeline and programs
        // STATE_BASE_ADDRESS (Milestone 2, steps 1-2) — still no drawing yet.

        rcs_error_reporting_init();

        if (!select_pipeline(PIPELINE_SELECT::PIPELINE_3D)) {
            Log::info("intel-rcs: pipeline select failed");
            return false;
        }

        if (!state_base_address_setup()) {
            Log::info("intel-rcs: STATE_BASE_ADDRESS setup failed");
            return false;
        }

        constexpr u32 SCISSOR_OFFSET = 0x200; // 32-Byte aligned
        constexpr u32 SF_CLIP_OFFSET = 0x240; // 64-Byte aligned (0x240 % 64 == 0)
        constexpr u32 CC_OFFSET = 0x280;      // 32-Byte aligned
        constexpr u32 BLEND_STATE_OFFSET = 0x2C0;

        if (!setup_scissor_state(1920, 1080, SCISSOR_OFFSET)) {
            Log::info("intel-rcs: setup scissor state failed");
            return false;
        }

        if (!setup_viewport_state(0.0f, 0.0f, 1920.0f, 1080.0f, SF_CLIP_OFFSET, CC_OFFSET)) {
            Log::info("intel-rcs: setup viewport state failed");
            return false;
        }

        if (!setup_blend_state(BLEND_STATE_OFFSET)) {
            Log::info("intel-rcs: setup blend state failed");
            return false;
        }

        if (!vertex_buffer_setup()) {
            Log::log_dbc("intel-rcs: vertex buffer setup failed");
            return false;
        }

        u8* inst_base_cpu = static_cast<u8*>(virt_ptr(state_base_cpu_addr_));
        ShaderOffsets offsets = upload_shaders(inst_base_cpu);
        Log::debug("offsets: %u %u", offsets.ps_offset, offsets.vs_offset);

        if (!setup_shaders_and_pipeline(offsets)) {
            Log::info("intel-rcs: VS/PS setup failed");
            return false;
        }

        if (!render_target_setup(1920, 1080)) {
            Log::info("intel-rcs: render target setup failed");
            return false;
        }

        debug_dump_error_regs("baseline, before draw");
        debug_dump_render_target(1920, 1080, 1920 * 4);

        if (!draw_triangle()) {
            Log::info("intel-rcs: draw triangle failed");
            debug_dump_error_regs("after failed draw_triangle");
            dump_pipeline_stats("after draw fail");
            return false;
        }

        debug_dump_error_regs("after draw_triangle");
        debug_dump_render_target(1920, 1080, 1920 * 4);

        dump_pipeline_stats("after draw");

        debug_dump_sf_clip_viewport_and_scissor(SF_CLIP_OFFSET, SCISSOR_OFFSET);

        struct RawReg { u32 raw; };
        RawReg reg_head = engine_reg_read<RawReg>(ENGINE_RING_HEAD_OFF);
        RawReg reg_tail = engine_reg_read<RawReg>(ENGINE_RING_TAIL_OFF);

        Log::debug("RING HEAD: 0x%llx RING TAIL: 0x%llx", reg_head.raw, reg_tail.raw);

        Log::info("intel-rcs: Milestone 2 COMPLETE! Triangle Primitive dispatched to RCS Engine.");
        return true;
    }

    void IntelRcs::rcs_interrupts_enable() const {
        volatile RCS_IMR_REG& imr = *reinterpret_cast<volatile RCS_IMR_REG*>(engine_regs() + ENGINE_IMR_OFF);

        RCS_IMR_REG rcs_imr{};
        rcs_imr.bits.user_irq = 1;
        rcs_imr.bits.pipe_control_notify = 0;
        rcs_imr.bits.master_error = 0;
        rcs_imr.bits.timeout = 1;
        rcs_imr.bits.page_fault = 0;
        rcs_imr.bits.ctx_switch = 1;
        rcs_imr.bits.invalid_tile = 1;
        rcs_imr.bits.l3_counter = 1;
        rcs_imr.bits.wait_sem = 1;
        imr.raw = rcs_imr.raw;
        MMIO_POST_WRITE(rcs_imr);
    }

    void IntelRcs::rcs_error_reporting_init() const {
        volatile auto* eir = reinterpret_cast<volatile EIR_REG*>(engine_regs() + EIR_OFFSET);
        volatile auto* emr = reinterpret_cast<volatile EMR_REG*>(engine_regs() + EMR_OFFSET);

        // Unmask (0 = unmasked) both hardware-detected error sources so
        // they propagate from ESR into EIR: Instruction Error (fatal,
        // requires reset — but we want to SEE it, not have it silently
        // masked) and Command Privilege Violation.
        EIR_REG eir_val{};
        eir_val.rcs_error_bits.instruction_error = 0;
        eir_val.rcs_error_bits.privilege_violation = 0;
        eir_val.mask = 0xFFFFu;
        eir->raw = eir_val.raw;
        MMIO_POST_WRITE((*eir));

        EMR_REG emr_val{};
        emr_val.error_mask = 0x00;    // 0 = unmasked, propagate all error bits into EIR
        emr_val.reserved = 0xFFFFFFu; // MUST be written as 1 per PRM
        emr->raw = emr_val.raw;
        MMIO_POST_WRITE((*emr));
    }

    void IntelRcs::debug_dump_error_regs(const char* label) const {
        auto* eir = reinterpret_cast<volatile EIR_REG*>(engine_regs() + EIR_OFFSET);
        auto* emr = reinterpret_cast<volatile EMR_REG*>(engine_regs() + EMR_OFFSET);
        auto* esr = reinterpret_cast<volatile ESR_REG*>(engine_regs() + ESR_OFFSET);

        const u32 eir_raw = eir->raw;
        const u32 emr_raw = emr->raw;
        const u32 esr_raw = esr->raw;

        Log::info("=== RCS ERROR REGS [%s] ===", label);
        Log::info("  EIR=0x%08x (instr_error=%u priv_violation=%u)",
                  eir_raw, eir->rcs_error_bits.instruction_error, eir->rcs_error_bits.privilege_violation);
        Log::info("  EMR=0x%08x (error_mask=0x%02x)", emr_raw, emr->error_mask);
        Log::info("  ESR=0x%08x (instr_error=%u priv_violation=%u)",
                  esr_raw, esr->rcs_error_bits.instruction_error, esr->rcs_error_bits.privilege_violation);

        if (eir_raw != 0 || (esr_raw & 0xFFFFu) != 0) {
            Log::warning("intel-rcs: HARDWARE ERROR DETECTED (EIR or ESR non-zero) — see values above");
        }
    }

    //
    bool IntelRcs::select_pipeline(PIPELINE_SELECT::PipelineSelection mode) {
        const PIPELINE_SELECT cmd = PIPELINE_SELECT::create(mode);
        ring_write_cmd(cmd);
        const u32 target_seqno = seqno_next();
        emit_flush(target_seqno);

        ring_flush();

        return seqno_wait(target_seqno, 5'000'000, completion_flag_);
    }


    bool IntelRcs::state_base_address_setup() {
        // Legacy ring-buffer bring-up (no PPGTT): every base address below
        // points into the same GGTT-backed page range. This is intentionally
        // coarse for Milestone 2 — General/Surface/Dynamic/Instruction state
        // don't yet exist as separate allocations. Once real state objects
        // (surface state, binding tables, shader kernels) are introduced,
        // split this into dedicated per-purpose allocations sized to need.
        auto alloc = ggtt().alloc_persistent(STATE_BASE_PAGES, (1ULL << CacheDisabled), MOCS_UNCACHED);
        state_base_cpu_addr_ = alloc.cpu_addr;
        state_base_gfx_addr_ = alloc.gfx_addr;

        memset(virt_ptr(state_base_cpu_addr_), 0, STATE_BASE_PAGES * PAGE_SIZE);

        const u64 base = gfx_raw(state_base_gfx_addr_);

        STATE_BASE_ADDRESS cmd = STATE_BASE_ADDRESS::create();
        cmd.set_general_state(base, STATE_BASE_PAGES, MOCS_UNCACHED);
        cmd.set_surface_state(base, MOCS_UNCACHED);
        cmd.set_dynamic_state(base, STATE_BASE_PAGES, MOCS_UNCACHED);
        cmd.set_instruction_state(base, STATE_BASE_PAGES, MOCS_UNCACHED);
        // Indirect Object Base and Bindless Surface State intentionally left
        // with modify_enable=0 — not needed until indirect draws / bindless
        // resources are used.

        ring_write_cmd(cmd);
        const u32 target_seqno = seqno_next();
        emit_flush(target_seqno);

        ring_flush();

        const bool ok = seqno_wait(target_seqno, 5'000'000, completion_flag_);

        Log::info("intel-rcs: STATE_BASE_ADDRESS base GFX=0x%llx (%u pages)", base, STATE_BASE_PAGES);

        return ok;
    }

    bool IntelRcs::setup_shaders_and_pipeline(const ShaderOffsets& offsets) {
        // --- 0. 3DSTATE_URB_VS/HS/DS/GS ---
        // Per PRM programming note (repeated identically on all four URB
        // commands): "When programming <STAGE> URB state for the RCS 3D
        // pipe, the other three 3DSTATE_URB_* commands must also be
        // programmed in order for the programming of this state to be
        // valid." All four are REQUIRED together even though this driver
        // only uses VS — HS/DS/GS stay Function-Enable=0 elsewhere
        // (3DSTATE_HS/DS/GS, not sent here since Gen9 defaults those
        // disabled), but their URB allocation fields are "always used
        // (even if <STAGE> Function Enable is DISABLED)" per PRM. Without
        // this, VS URB output (gl_Position) may have nowhere valid to
        // land — the VS kernel runs, but its URB write target was never
        // actually allocated.
        //
        // Total on-chip URB is small (Gen9.5 GT2: ~64KB = 8 rows of 8KB);
        // HS/DS/GS get the PRM-mandated minimum (0 entries, since none of
        // them are enabled — "Only if <STAGE> is disabled can this field
        // be programmed to 0"), VS gets the rest. Addresses are in 8KB
        // units (urb_starting_address), sizes in 512-bit units
        // (urb_entry_allocation_size).
        STATE_URB_HS urb_hs = STATE_URB_HS::create();
        urb_hs.urb_starting_address = 0;
        urb_hs.urb_entry_allocation_size = 0; // 1 512-bit row (0 = 1-1=0 encoding)
        urb_hs.number_of_urb_entries = 0;     // HS disabled — 0 is the mandated value
        ring_write_cmd(urb_hs);

        STATE_URB_DS urb_ds = STATE_URB_DS::create();
        urb_ds.urb_starting_address = 0;
        urb_ds.urb_entry_allocation_size = 0;
        urb_ds.number_of_urb_entries = 0; // DS disabled — 0 is the mandated value
        ring_write_cmd(urb_ds);

        STATE_URB_GS urb_gs = STATE_URB_GS::create();
        urb_gs.urb_starting_address = 0;
        urb_gs.urb_entry_allocation_size = 0;
        urb_gs.number_of_urb_entries = 0; // GS disabled — 0 is the mandated value
        ring_write_cmd(urb_gs);

        // VS is the only stage actually producing URB output here
        // (gl_Position, 2x 256-bit units per vertex per
        // vertex_urb_entry_output_length below). Starting address 0,
        // entry size matches vertex_urb_entry_output_length (2 = 2 512-bit-
        // equivalent... actually PRM units for URB_VS allocation size are
        // 512-bit rows, distinct from the 256-bit units used by
        // vertex_urb_entry_output_length in 3DSTATE_VS — sized generously
        // here at 1 row (0-encoded) per entry, well within a single 512-bit
        // row for a 2x256-bit (= 1x512-bit) output.
        STATE_URB_VS urb_vs = STATE_URB_VS::create();
        urb_vs.urb_starting_address = 0;
        urb_vs.urb_entry_allocation_size = 3; // 1 512-bit row per entry (0 = 1-1 encoding)
        urb_vs.number_of_urb_entries = 64;    // within [64,1856] legal range, minimum valid value
        ring_write_cmd(urb_vs);

        STATE_GS gs_cmd = STATE_GS::create_disabled();
        ring_write_cmd(gs_cmd);

        STATE_HS hs_cmd = STATE_HS::create_disabled();
        ring_write_cmd(hs_cmd);

        STATE_TE te_cmd = STATE_TE::create_disabled();
        ring_write_cmd(te_cmd);

        STATE_DS ds_cmd = STATE_DS::create_disabled();
        ring_write_cmd(ds_cmd);

        // --- 1. 3DSTATE_VS ---
        STATE_VS vs_cmd = STATE_VS::create();
        vs_cmd.kernel_start_pointer = (offsets.vs_offset >> 6); // Bitfield [63:6]
        vs_cmd.function_enable = 1;
        vs_cmd.simd8_dispatch_enable = 1;
        vs_cmd.vertex_urb_entry_read_length = 1;             // 1x 256-Bit Unit aus VF (Positions-Vector)
        vs_cmd.vertex_urb_entry_output_length = 8;           // 8× 256-bit = 2048 bits = 256 bytes, matches actual GRF output (g118-g125)
        vs_cmd.dispatch_grf_start_register_for_urb_data = 2;
        vs_cmd.maximum_number_of_threads = 64 - 1;           // Gen9 Thread Limit Limitierung

        ring_write_cmd(vs_cmd);

        // --- 1b. 3DSTATE_CLIP / 3DSTATE_SF ---
        // Fixed-function Clipper and Setup/Strip-Fan stages, sitting
        // between the geometry stages (VS/GS/HS/DS/TE, all above) and the
        // Windower/PS below. Both stages come up in an unknown/possibly
        // hostile reset state — in particular STATE_CLIP's reset default
        // is NOT guaranteed to be a harmless pass-through, and
        // STATE_SF::viewport_transform_enable defaults to 0, meaning clip-
        // space coordinates are never mapped into screen space at all.
        // Without these two, the clipper/setup stage can silently drop
        // every primitive before it ever reaches the Windower/PS — exactly
        // matching VS Invocations > 0 but PS Invocations == 0.
        STATE_CLIP clip_cmd = STATE_CLIP::create_default();
        clip_cmd.clipper_statistics_enable = 1;
        ring_write_cmd(clip_cmd);

        STATE_SF sf_cmd = STATE_SF::create_default();
        sf_cmd.viewport_transform_enable = 1;
        sf_cmd.statistics_enable = 1;
        ring_write_cmd(sf_cmd);

        // --- 2. 3DSTATE_PS ---
        STATE_PS ps_cmd = STATE_PS::create();
        ps_cmd.kernel_start_pointer_0 = offsets.ps_offset >> 6; // FIX: war ohne >> 6
        ps_cmd.dispatch_8_pixel_enable = 1;
        ps_cmd.dispatch_16_pixel_enable = 0; // kein SIMD16-Kernel im Ring
        ps_cmd.vector_mask_enable = 0;       // FIX: war 1, darf 0 sein ohne SIMD16
        ps_cmd.dispatch_grf_start_0 = 2;     // Mesa setzt 2 (g0/g1 = header, g2+ = payload)
        ps_cmd.maximum_number_of_threads = 64 - 1;
        // ps_cmd.attribute_enable            = 0;
        ring_write_cmd(ps_cmd);

        // --- 2c. 3DSTATE_PS_EXTRA ---
        // Per PRM: "When [Pixel Shader Valid] is clear the rest of this
        // command should also be clear" — i.e. without pixel_shader_valid=1
        // the pipeline treats the PS stage as absent, REGARDLESS of what
        // 3DSTATE_PS/3DSTATE_WM say. This was almost certainly why nothing
        // rendered despite a correctly-configured 3DSTATE_PS: the PS stage
        // itself was never marked present.
        STATE_PS_EXTRA ps_extra_cmd = STATE_PS_EXTRA::create();
        ps_extra_cmd.pixel_shader_valid = 1;
        ps_extra_cmd.pixel_shader_does_not_write_to_rt = 0; // our kernel DOES write to RT
        ps_extra_cmd.pixel_shader_computed_depth_mode = STATE_PS_EXTRA::PSCDEPTH_OFF;
        ps_extra_cmd.pixel_shader_kills_pixel = 0;
        ps_extra_cmd.pixel_shader_uses_source_depth = 0;
        ps_extra_cmd.pixel_shader_uses_source_w = 0;
        ps_extra_cmd.input_coverage_mask_state = STATE_PS_EXTRA::INPUT_COVERAGE_NONE;
        ring_write_cmd(ps_extra_cmd);

        // --- 2d. 3DSTATE_PS_BLEND ---
        // Per IHD-OS-ICLLP-Vol 9 (WM_INT::ThreadDispatchEnable formula):
        // dispatch requires PixelShaderValid AND (!PixelShaderDoesNotWriteRT
        // && HasWriteableRT). The reset default for HasWriteableRT is 0 —
        // without this command, the PS stage would never dispatch even
        // with pixel_shader_valid=1 set above. No blending/alpha-test
        // needed for a simple constant-color kernel.
        STATE_PS_BLEND ps_blend_cmd = STATE_PS_BLEND::create_simple_writeable_rt();
        ring_write_cmd(ps_blend_cmd);

        // --- 2b. 3DSTATE_WM ---
        // Windower/IZ fixed-function stage. Without this, the pipeline's
        // early-depth/dispatch-enable logic and barycentric interpolation
        // wiring are left in an undefined/reset state — 3DSTATE_PS alone
        // only configures the PS thread dispatcher itself, not whether the
        // Windower actually feeds it. Defaults here are the simplest valid
        // configuration for a depth-test-free, non-interpolating constant-
        // color kernel: normal (not forced) dispatch, no early depth/
        // stencil special-casing, perspective-pixel barycentric (the one
        // mode that always needs to be present even if the kernel ignores
        // it), Z/W evaluated at pixel center.
        STATE_WM wm_cmd = STATE_WM::create();
        wm_cmd.force_kill_pixel_enable = STATE_WM::KILL_NORMAL;
        wm_cmd.force_thread_dispatch_enable = STATE_WM::DISPATCH_FORCE_ON;
        wm_cmd.early_depth_stencil_control = STATE_WM::EARLY_DS_NORMAL;
        wm_cmd.position_zw_interpolation_mode = STATE_WM::INTERP_PIXEL;
        wm_cmd.barycentric_interpolation_mode = STATE_WM::BARY_PERSPECTIVE_PIXEL;
        wm_cmd.statistics_enable = 1;
        ring_write_cmd(wm_cmd);

        // --- 2e. 3DSTATE_SBE ---
        // Routes attributes from SF/URB to the WM/PS threads. Our PS kernel
        // (gen9_ps_kernel_simd8) is a pure constant-color kernel — it reads
        // no interpolated attributes at all, so num_attributes=0 here.
        // However, per PRM: "It is UNDEFINED to set [Vertex URB Entry Read
        // Length] to 0 indicating no Vertex URB data to be read" — valid
        // range is [1,16], so urb_read_length must stay at the minimum (1)
        // even though nothing is actually consumed downstream.
        STATE_SBE sbe_cmd = STATE_SBE::create_default(/* num_attributes */ 0, /* urb_read_length */ 2);
        ring_write_cmd(sbe_cmd);

        // --- 2e-pre. 3DSTATE_MULTISAMPLE ---
        // Required Gen9 pipeline state — was previously never programmed
        // (undefined/reset value). Single-sample rendering, pixel center
        // sampling: the standard non-MSAA configuration. Windower dispatch
        // logic derives its behavior partly from this state; leaving it
        // unprogrammed can leave WM in a state where it never dispatches
        // pixels even for a primitive that already cleared CLIP/SF —
        // matching CL Primitives:1 / PS Invocations:0.
        STATE_MULTISAMPLE ms_cmd = STATE_MULTISAMPLE::create_default(
            STATE_MULTISAMPLE::NUMSAMPLES_1, STATE_MULTISAMPLE::CENTER
        );
        ring_write_cmd(ms_cmd);

        // --- 2e-pre2. 3DSTATE_WM_DEPTH_STENCIL ---
        // No depth buffer is bound (no 3DSTATE_DEPTH_BUFFER anywhere in
        // this driver yet) — per PRM, enabling Depth Buffer Write with no
        // depth buffer defined is explicitly UNDEFINED behavior, and an
        // enabled depth test against a nonexistent buffer is equally
        // unspecified. Both depth test and depth write must stay disabled
        // until 3DSTATE_DEPTH_BUFFER is added. This was previously left at
        // its (likely enabled) reset default — a plausible reason WM was
        // silently dropping the primitive before dispatching to PS.
        STATE_WM_DEPTH_STENCIL wm_ds_cmd = STATE_WM_DEPTH_STENCIL::create_default(
            false,
            false
        );
        ring_write_cmd(wm_ds_cmd);


        // --- 2f. 3DSTATE_RASTER ---
        // CULL_NONE is the hardware reset default per PRM, so this alone
        // shouldn't explain "CL Invocations > 0, CL Primitives: 0" — but we
        // program it explicitly rather than relying on reset state, and
        // more importantly: create_default() also enables both viewport Z
        // near/far clip tests. Our triangle sits at Z=0.0 on all three
        // vertices; if that lands exactly on (or outside, due to rounding)
        // the near-plane boundary given our CC_VIEWPORT min/max depth
        // range, the clip test would silently reject the primitive here —
        // matching our symptom exactly. Disabling both Z clip tests
        // isolates that as a variable; re-enable once confirmed working.
        STATE_RASTER raster_cmd = STATE_RASTER::create_default(STATE_RASTER::CULL_NONE, STATE_RASTER::FRONTWINDING_CCW);
        raster_cmd.viewport_z_near_clip_test_enable = 0;
        raster_cmd.viewport_z_far_clip_test_enable = 0;
        ring_write_cmd(raster_cmd);

        // 3DSTATE_SAMPLE_MASK
        STATE_SAMPLE_MASK sample_mask_cmd = STATE_SAMPLE_MASK::create();
        ring_write_cmd(sample_mask_cmd);

        STATE_VF_STATISTICS vf_stats = STATE_VF_STATISTICS::create_enabled();
        ring_write_cmd(vf_stats);

        PIPE_CONTROL pc = PIPE_CONTROL::create();
        pc.command_streamer_stall_enable = 1;
        pc.instruction_cache_invalidate_enable = 1;
        pc.state_cache_invalidation_enable = 1;
        ring_write_cmd(pc);

        const u32 seqno = seqno_next();
        emit_flush(seqno);
        ring_flush();
        return seqno_wait(seqno, 5'000'000, completion_flag_);
    }

    bool IntelRcs::draw_triangle(u32 width, u32 height) {
        constexpr STATE_VF_TOPOLOGY topo = STATE_VF_TOPOLOGY::create(PRIM_3D_TRILIST);
        ring_write_cmd(topo);

        // 1. Set 3DSTATE_DRAWING_RECTANGLE (Klipp-Rechteck auf Bildschirmgröße setzen)
        const DRAWING_RECTANGLE rect = DRAWING_RECTANGLE::create_full(width - 5, height - 5);
        ring_write_cmd(rect);

        // 2. Set 3DPRIMITIVE (3 Vertices, sequentieller Speicherzugriff, 1 Instanz)
        CMD_3DPRIMITIVE prim = CMD_3DPRIMITIVE::create();
        prim.indirect_parameter_enable = 0;
        prim.primitive_topology_type = PRIM_3D_TRILIST;
        prim.vertex_access_type = CMD_3DPRIMITIVE::ACCESS_SEQUENTIAL;
        prim.vertex_count_per_instance = 3;
        prim.start_vertex_location = 0;
        prim.instance_count = 1;
        prim.start_instance_location = 0;
        prim.base_vertex_location = 0;

        ring_write_cmd(prim);

        // 3. Flush Cache & Wait for Completion via HWSP Seqno
        const u32 target_seqno = seqno_next();
        emit_flush(target_seqno);

        ring_flush();

        const bool ok = seqno_wait(target_seqno, 5'000'000, completion_flag_);

        Log::info("intel-rcs: 3DPRIMITIVE dispatched (3 vertices, %ux%u viewport)", width, height);

        return ok;
    }

    // Offset im Dynamic State Speicher (32-Byte aligniert)
    bool IntelRcs::setup_scissor_state(u32 width, u32 height, u32 dynamic_offset) {
        // 1. CPU-Zeiger auf den gemeinsamen State-Memory holen
        u8* base_ptr = reinterpret_cast<u8*>(virt_ptr(state_base_cpu_addr_));

        // 2. SCISSOR_RECT an die gewünschte Stelle im Speicher schreiben
        SCISSOR_RECT rect = SCISSOR_RECT::create_full(width, height);
        memcpy(base_ptr + dynamic_offset, &rect, sizeof(SCISSOR_RECT));

        // 3. Cache flushen (falls der Speicher gecacht ist)
        asm volatile("clflush (%0)" :: "r"(base_ptr + dynamic_offset) : "memory");

        // 4. Command mit relativer Adresse erstellen und in den Ring-Buffer schreiben
        SCISSOR_STATE_POINTERS cmd = SCISSOR_STATE_POINTERS::create(dynamic_offset);
        ring_write_cmd(cmd);

        Log::info("intel-rcs: Scissor state applied at offset 0x%x (%ux%u)", dynamic_offset, width, height);
        return true;
    }

    bool IntelRcs::setup_viewport_state(
        float x, float y, float width, float height, u32 sf_clip_offset, u32 cc_offset
    ) {
        u8* base_ptr = static_cast<u8*>(virt_ptr(state_base_cpu_addr_));

        // 1. SF_CLIP_VIEWPORT (64-byte aligned offset)
        if (sf_clip_offset % 64 != 0) {
            Log::info("intel-rcs: sf_clip_offset (0x%x) is not 64-byte aligned!", sf_clip_offset);
            return false;
        }

        SF_CLIP_VIEWPORT sf_vp = SF_CLIP_VIEWPORT::create_full_screen(x, y, width, height);
        memcpy(base_ptr + sf_clip_offset, &sf_vp, sizeof(SF_CLIP_VIEWPORT));
        asm volatile("clflush (%0)" :: "r"(base_ptr + sf_clip_offset) : "memory");

        VIEWPORT_POINTERS_SF_CLIP sf_cmd = VIEWPORT_POINTERS_SF_CLIP::create(sf_clip_offset);
        ring_write_cmd(sf_cmd);

        // 2. CC_VIEWPORT (32-byte aligned offset)
        if (cc_offset % 32 != 0) {
            Log::info("intel-rcs: cc_offset (0x%x) is not 32-byte aligned!", cc_offset);
            return false;
        }

        CC_VIEWPORT cc_vp = CC_VIEWPORT::create();
        memcpy(base_ptr + cc_offset, &cc_vp, sizeof(CC_VIEWPORT));
        asm volatile("clflush (%0)" :: "r"(base_ptr + cc_offset) : "memory");

        VIEWPORT_POINTERS_CC cc_cmd = VIEWPORT_POINTERS_CC::create(cc_offset);
        ring_write_cmd(cc_cmd);

        Log::info("intel-rcs: Viewport states applied (SF at 0x%x, CC at 0x%x)", sf_clip_offset, cc_offset);
        return true;
    }

    bool IntelRcs::setup_blend_state(u32 dynamic_offset) {
        if (dynamic_offset % 64 != 0) {
            Log::info("intel-rcs: blend_state offset (0x%x) is not 64-byte aligned!", dynamic_offset);
            return false;
        }

        u8* base_ptr = static_cast<u8*>(virt_ptr(state_base_cpu_addr_));

        // Single RT, opaque, no blending — matches our PS output (solid
        // red, no alpha blending needed)
        BLEND_STATE blend = BLEND_STATE::create_single_rt_opaque();
        memcpy(base_ptr + dynamic_offset, &blend, sizeof(BLEND_STATE));
        asm volatile("clflush (%0)" :: "r"(base_ptr + dynamic_offset) : "memory");

        STATE_BLEND_STATE_POINTERS cmd = STATE_BLEND_STATE_POINTERS::create_pointer(dynamic_offset);
        ring_write_cmd(cmd);

        Log::info("intel-rcs: Blend state applied at offset 0x%x (single RT, opaque)", dynamic_offset);
        return true;
    }


    u32 IntelRcs::gt_user_irq_bit() const {
        return GT0_RCS_PIPE_CONTROL_NOTIFY_BIT;
    }

    u32 IntelRcs::gt_debug_irq_bitmask() const {
        // Unmask page_fault (bit 7) and master_error (bit 3) purely for
        // visibility in device_irq_handler()'s diagnostic logging — see
        // interrupt_regs.h RCS_ICR_BITS. Neither drives
        // on_gt_user_interrupt(); completion still only comes from
        // pipe_control_notify (gt_user_irq_bit()). Without this, a silent
        // page fault on a bad surface/binding-table/kernel address would
        // never surface anywhere: seqno_wait() would just time out with no
        // explanation, because it only watches for pipe_control_notify.
        constexpr u32 RCS_PAGE_FAULT_BIT = 7;
        constexpr u32 RCS_MASTER_ERROR_BIT = 3;
        return (1u << RCS_PAGE_FAULT_BIT) | (1u << RCS_MASTER_ERROR_BIT);
    }


    void IntelRcs::on_gt_user_interrupt() {
        completion_flag_.set();
    }

    void IntelRcs::emit_flush(u32 seqno) {
        PIPE_CONTROL flush_cmd = PIPE_CONTROL::create();

        flush_cmd.command_streamer_stall_enable = 1;
        flush_cmd.render_target_cache_flush_enable = 1;
        flush_cmd.depth_cache_flush_enable = 1;
        flush_cmd.dc_flush_enable = 1;

        ring_write_cmd(flush_cmd);

        PIPE_CONTROL inv_cmd = PIPE_CONTROL::create();

        inv_cmd.command_streamer_stall_enable = 1;
        inv_cmd.state_cache_invalidation_enable = 1;     // State Cache (L1/L2)
        inv_cmd.texture_cache_invalidation_enable = 1;   // Texture Cache
        inv_cmd.instruction_cache_invalidate_enable = 1; // Instruction Cache (EU)
        inv_cmd.constant_cache_invalidation_enable = 1;  // Push Constant Cache
        // inv_cmd.vf_cache_invalidation_enable = 1;        // Vertex Fetch Cache
        inv_cmd.tlb_invalidate = 1; // Render Engine TLBs

        const u64 hwsp_seqno_addr = gfx_raw(hwsp_gfx_addr_) + HWSP_SEQNO_OFFSET;
        inv_cmd.store_data_index = 0; // Wir übergeben jetzt eine volle Adresse, kein Page-Index mehr
        inv_cmd.set_write_immediate(hwsp_seqno_addr, seqno, /* use_ggtt */ true);

        inv_cmd.notify_enable = 1;

        ring_write_cmd(inv_cmd);

        /*MI_USER_INTERRUPT_CMD ui{};
        ui.opcode = OPCODE_MI_USER_INTERRUPT;
        ui.client = CLIENT_MI;
        ring_write_cmd(ui);*/
    }

    bool IntelRcs::vertex_buffer_setup() {
        // Three vertices, float3 position only (12 bytes/vertex). Clip-space
        // coordinates (-1..1 on X/Y): a simple upward-pointing triangle,
        // centered, Z=0. Color is intentionally NOT part of the vertex here
        // — it'll come from a constant in the pixel shader once that's
        // wired up (step 4), keeping this vertex layout minimal for now.
        struct Vertex {
            float x, y, z;
        };
        static constexpr Vertex triangle[3] = {
            {0.0f, 0.5f, 0.0f},
            {-0.5f, -0.5f, 0.0f},
            {0.5f, -0.5f, 0.0f},
        };
        constexpr u32 vertex_stride = sizeof(Vertex);
        constexpr u32 vertex_buffer_size = sizeof(triangle);

        // Allocate + fill the vertex buffer. Same alloc_persistent pattern
        // as hwsp_alloc()/state_base_address_setup(). Uncached: this is a
        // tiny, one-shot CPU-written buffer, not a texture — no benefit from
        // caching, and uncached sidesteps any manual cache-flush-before-GPU-
        // read concerns during bring-up.
        auto alloc = ggtt().alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        vertex_buffer_cpu_addr_ = alloc.cpu_addr;
        vertex_buffer_gfx_addr_ = alloc.gfx_addr;

        memset(virt_ptr(vertex_buffer_cpu_addr_), 0, PAGE_SIZE);
        memcpy(virt_ptr(vertex_buffer_cpu_addr_), triangle, vertex_buffer_size);

        const u64 vb_addr = gfx_raw(vertex_buffer_gfx_addr_);

        // --- 3DSTATE_VERTEX_BUFFERS: one VB, slot 0 ---
        const VERTEX_BUFFERS_HEADER vb_header = VERTEX_BUFFERS_HEADER::create(1);
        const VERTEX_BUFFER_STATE vb_state =
            VERTEX_BUFFER_STATE::create(/* vb_index */ 0, vb_addr, vertex_stride, vertex_buffer_size, MOCS_UNCACHED);

        ring_write_cmd(vb_header);
        ring_write_cmd(vb_state);

        // --- 3DSTATE_VERTEX_ELEMENTS: one element, XYZ from VB0 + W=1.0 ---
        constexpr VERTEX_ELEMENTS_HEADER el_header = VERTEX_ELEMENTS_HEADER::create(1);
        constexpr VERTEX_ELEMENT_STATE el_state =
            VERTEX_ELEMENT_STATE::create_xyz_w1(/* vb_index */ 0, /* offset */ 0, SURFACE_FORMAT_R32G32B32_FLOAT);

        ring_write_cmd(el_header);
        ring_write_cmd(el_state);

        const u32 target_seqno = seqno_next();
        emit_flush(target_seqno);

        ring_flush();

        const bool ok = seqno_wait(target_seqno, 5'000'000, completion_flag_);

        Log::info(
            "intel-rcs: vertex buffer GFX=0x%llx (%u bytes, stride %u) + 1 vertex element programmed",
            vb_addr, vertex_buffer_size, vertex_stride
        );

        return ok;
    }

    bool IntelRcs::render_target_setup(u32 width, u32 height) {
        // --- 1. Separate offscreen render target buffer (NOT state_base_*
        //        — that's pipeline state; this is actual pixel data). BCS
        //        will blit this to the visible framebuffer afterwards,
        //        entirely independent of RCS. B8G8R8A8_UNORM: 4 bytes/pixel,
        //        matches the common display/render-target format from
        //        surface_format.h.
        constexpr u32 bytes_per_pixel = 4;
        const u32 pitch = width * bytes_per_pixel;
        const u32 rt_size = pitch * height;
        const u32 rt_pages = (rt_size + PAGE_SIZE - 1) / PAGE_SIZE;

        auto rt_alloc = ggtt().alloc_persistent(rt_pages, (1ULL << CacheDisabled), MOCS_UNCACHED);
        render_target_cpu_addr_ = rt_alloc.cpu_addr;
        render_target_gfx_addr_ = rt_alloc.gfx_addr;

        memset(virt_ptr(render_target_cpu_addr_), 0, rt_pages * PAGE_SIZE);

        const u64 rt_addr = gfx_raw(render_target_gfx_addr_);

        // --- 2. RENDER_SURFACE_STATE + BINDING_TABLE_STATE, both Surface
        //        State Base Address-relative (same physical base as
        //        Dynamic/Instruction State in this driver's coarse
        //        single-buffer STATE_BASE_ADDRESS setup — see
        //        state_base_address_setup()). Placed at fixed offsets that
        //        don't collide with the shader kernels (0x0-0x100) or
        //        scissor/viewport (0x200-0x288).
        static constexpr u32 SURFACE_STATE_OFFSET = 0x300; // 64-byte aligned
        static constexpr u32 BINDING_TABLE_OFFSET = 0x340; // 64-byte aligned

        u8* base_ptr = static_cast<u8*>(virt_ptr(state_base_cpu_addr_));

        const RENDER_SURFACE_STATE surface = RENDER_SURFACE_STATE::create_simple_2d(
            rt_addr, width, height, pitch, SURFACE_FORMAT_B8G8R8A8_UNORM, MOCS_UNCACHED
        );
        memcpy(base_ptr + SURFACE_STATE_OFFSET, &surface, sizeof(RENDER_SURFACE_STATE));
        asm volatile("clflush (%0)" ::"r"(base_ptr + SURFACE_STATE_OFFSET) : "memory");

        // Binding table index 0 -> our one render target surface, above.
        BINDING_TABLE_STATE bt_entry = BINDING_TABLE_STATE::create();
        bt_entry.surface_state_pointer = SURFACE_STATE_OFFSET >> 6;
        memcpy(base_ptr + BINDING_TABLE_OFFSET, &bt_entry, sizeof(BINDING_TABLE_STATE));
        asm volatile("clflush (%0)" ::"r"(base_ptr + BINDING_TABLE_OFFSET) : "memory");

        // --- 3. 3DSTATE_BINDING_TABLE_POINTERS_PS: tells the PS stage
        //        where binding table index 0 (used above) lives.
        STATE_BINDING_TABLE_POINTERS bt_ptr_cmd = STATE_BINDING_TABLE_POINTERS::create_ps();
        bt_ptr_cmd.pointer_to_binding_table = BINDING_TABLE_OFFSET >> 5;
        ring_write_cmd(bt_ptr_cmd);

        // --- 4. 3DSTATE_DEPTH_BUFFER (SURFTYPE_NULL) ---
        // We have no depth buffer and depth test/write are disabled in
        // 3DSTATE_WM_DEPTH_STENCIL. Without this command, HiZ/Early-Z
        // state is left at whatever GPU reset left it in — PRM explicitly
        // requires HiZ disabled for SURFTYPE_NULL, so this is the
        // documented way to declare "no depth surface" rather than
        // silently omitting the command and hoping reset state is benign.
        DEPTH_BUFFER depth_cmd = DEPTH_BUFFER::create_null();
        ring_write_cmd(depth_cmd);

        const u32 target_seqno = seqno_next();
        emit_flush(target_seqno);

        ring_flush();

        const bool ok = seqno_wait(target_seqno, 5'000'000, completion_flag_);

        Log::info(
            "intel-rcs: render target GFX=0x%llx (%ux%u, pitch %u) bound at binding table index 0",
            rt_addr, width, height, pitch
        );

        return ok;
    }

    void IntelRcs::debug_dump_render_target(u32 width, u32 height, u32 pitch) const {
        auto* pixels = static_cast<volatile u32*>(virt_ptr(render_target_cpu_addr_));

        const u32 stride_pixels = pitch / 4;
        const u32 cx = width / 2;
        const u32 cy = height / 2;

        auto sample = [&](const char* label, u32 x, u32 y) {
            const u32 val = pixels[y * stride_pixels + x];
            Log::info("intel-rcs: RT pixel [%s] (%u,%u) = 0x%08x", label, x, y, val);
        };

        Log::info("=== RENDER TARGET READBACK (%ux%u, pitch %u) ===", width, height, pitch);
        sample("center", cx, cy);
        sample("top-left", 0, 0);
        sample("top-right", width - 1, 0);
        sample("bottom-left", 0, height - 1);
        sample("bottom-right", width - 1, height - 1);

        // Full-screen scan: track a bounding box of every non-zero pixel plus a
        // handful of example values, instead of logging each hit individually
        // (which would be hundreds of thousands of lines at 1920x1080). This
        // tells us not just "did the PS write anything" but "where exactly,
        // and does that region roughly match the expected triangle" — useful
        // for catching viewport/NDC mapping bugs that put the triangle
        // somewhere other than screen-center.
        u32 min_x = width, max_x = 0;
        u32 min_y = height, max_y = 0;
        u64 nonzero_count = 0;

        static constexpr u32 MAX_EXAMPLES = 5;
        struct Example {
            u32 x, y, val;
        };
        Example examples[MAX_EXAMPLES]{};
        u32 example_count = 0;

        for (u32 y = 0; y < height; y++) {
            const u32 row_base = y * stride_pixels;
            for (u32 x = 0; x < width; x++) {
                const u32 val = pixels[row_base + x];
                if (val == 0) continue;

                nonzero_count++;
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;

                if (example_count < MAX_EXAMPLES) {
                    examples[example_count++] = {x, y, val};
                }
            }
        }

        if (nonzero_count == 0) {
            Log::warning("intel-rcs: RT full-screen scan found NO non-zero pixels (%ux%u)", width, height);
            return;
        }

        Log::info(
            "intel-rcs: RT full-screen scan: %llu non-zero pixels, bounding box (%u,%u)-(%u,%u) [%ux%u]",
            nonzero_count, min_x, min_y, max_x, max_y, max_x - min_x + 1, max_y - min_y + 1
        );

        for (u32 i = 0; i < example_count; i++) {
            Log::info(
                "intel-rcs: RT scan example[%u] (%u,%u) = 0x%08x", i, examples[i].x, examples[i].y, examples[i].val
            );
        }

        // Sanity check: our triangle in vertex_buffer_setup spans clip-space
        // roughly x:-0.5..0.5, y:-0.5..0.5, which a full-screen viewport maps to
        // a bounding box centered on-screen. Flag it if the actual bounding box
        // is way off-center — that points at a viewport/scissor/NDC bug even
        // though pixels *were* written.
        const u32 bbox_cx = (min_x + max_x) / 2;
        const u32 bbox_cy = (min_y + max_y) / 2;
        const u32 dx = bbox_cx > cx ? bbox_cx - cx : cx - bbox_cx;
        const u32 dy = bbox_cy > cy ? bbox_cy - cy : cy - bbox_cy;

        if (dx > width / 10 || dy > height / 10) {
            Log::warning(
                "intel-rcs: RT non-zero bounding box center (%u,%u) is far from screen center (%u,%u) — "
                "check viewport/scissor/NDC mapping",
                bbox_cx, bbox_cy, cx, cy
            );
        }
    }

    void IntelRcs::debug_dump_sf_clip_viewport_and_scissor(u32 sf_clip_offset, u32 scissor_offset) const {
        const u8* base_ptr = static_cast<const u8*>(virt_ptr(state_base_cpu_addr_));

        SF_CLIP_VIEWPORT sf_vp{};
        memcpy(&sf_vp, base_ptr + sf_clip_offset, sizeof(SF_CLIP_VIEWPORT));

        SCISSOR_RECT scissor{};
        memcpy(&scissor, base_ptr + scissor_offset, sizeof(SCISSOR_RECT));

        Log::info("=== SF_CLIP_VIEWPORT @ DYNAMIC_STATE_BASE_ADDRESS+0x%x ===", sf_clip_offset);
        Log::info("  m00=%f m11=%f m22=%f", sf_vp.m00, sf_vp.m11, sf_vp.m22);
        Log::info("  m30=%f m31=%f m32=%f", sf_vp.m30, sf_vp.m31, sf_vp.m32);
        Log::info(
            "  guardband: x[%f,%f] y[%f,%f]",
            sf_vp.x_min_clip_guardband, sf_vp.x_max_clip_guardband,
            sf_vp.y_min_clip_guardband, sf_vp.y_max_clip_guardband
        );
        Log::info(
            "  viewport (screen-space): x[%f,%f] y[%f,%f]",
            sf_vp.x_min_viewport, sf_vp.x_max_viewport,
            sf_vp.y_min_viewport, sf_vp.y_max_viewport
        );

        Log::info("=== SCISSOR_RECT @ DYNAMIC_STATE_BASE_ADDRESS+0x%x ===", scissor_offset);
        Log::info(
            "  x[%u,%u] y[%u,%u]",
            scissor.x_min, scissor.x_max, scissor.y_min, scissor.y_max
        );
    }

    bool IntelRcs::present_to_screen(u32 width, u32 height) const {
        if (!bcs_) return false;
        Log::log_dbc("presenting to screen");

        const u32 pitch = width * 4;
        bcs_->composite_gpu_surface(render_target_gfx_addr_, pitch, width, height);
        bcs_->present();
        return true;
    }

    void IntelRcs::dump_pipeline_stats(const char* label) const {
        const u64 ia_vertices = engine_reg_read64(IA_VERTICES_COUNT_OFFSET);
        const u64 ia_primitives = engine_reg_read64(IA_PRIMITIVES_COUNT_OFFSET);
        const u64 vs_invocations = engine_reg_read64(VS_INVOCATION_COUNT_OFFSET);
        const u64 cl_invocations = engine_reg_read64(CL_INVOCATION_COUNT_OFFSET);
        const u64 cl_primitives = engine_reg_read64(CL_PRIMITIVES_COUNT_OFFSET);
        const u64 ps_invocations = engine_reg_read64(PS_INVOCATION_COUNT_OFFSET);
        const u64 ps_depth_pass = engine_reg_read64(PS_DEPTH_COUNT_OFFSET);

        Log::info("=== RCS PIPELINE STATS [%s] ===", label);
        Log::info("  IA Vertices:    %llu", ia_vertices);
        Log::info("  IA Primitives:  %llu", ia_primitives);
        Log::info("  VS Invocations: %llu", vs_invocations);
        Log::info("  CL Invocations: %llu", cl_invocations);
        Log::info("  CL Primitives:  %llu", cl_primitives);
        Log::info("  PS Invocations: %llu", ps_invocations);
        Log::info("  PS Depth Pass:  %llu", ps_depth_pass);
    }
} // namespace blt
