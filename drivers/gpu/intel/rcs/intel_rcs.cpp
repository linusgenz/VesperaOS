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
#include <gpu/intel/regs/gt_interrupt_regs.h>
#include <vespera/log.h>
#include <vespera/graphics/display_types.h>
#include "gen9_kernels.h"
#include "gen9_vs_kernel_mvp.h"
#include "gfx_pipeline_stats_regs.h"
#include "mat4.h"
#include "commands/cmd_3dprimitive.h"
#include "commands/cmd_3dstate_binding_table.h"
#include "commands/cmd_3dstate_blend_state_pointers.h"
#include "commands/cmd_3dstate_clip.h"
#include "commands/cmd_3dstate_constant_vs.h"
#include "commands/cmd_3dstate_depth_buffer.h"
#include "commands/cmd_3dstate_drawing_rectangle.h"
#include "commands/cmd_3dstate_ds.h"
#include "commands/cmd_3dstate_gs.h"
#include "commands/cmd_3dstate_hs.h"
#include "commands/cmd_3dstate_multisample.h"
#include "commands/cmd_3dstate_ps.h"
#include "commands/cmd_3dstate_ps_blend.h"
#include "commands/cmd_3dstate_raster.h"
#include "commands/cmd_3dstate_sample_mask.h"
#include "commands/cmd_3dstate_sbe.h"
#include "commands/cmd_3dstate_sf.h"
#include "commands/cmd_3dstate_te.h"
#include "commands/cmd_3dstate_urb.h"
#include "commands/cmd_3dstate_vf_statistics.h"
#include "commands/cmd_3dstate_vf_topology.h"
#include "commands/cmd_3dstate_vs.h"
#include "commands/cmd_3dstate_wm.h"
#include "commands/cmd_3dstate_wm_depth_stencil.h"
#include "commands/cmd_pipeline_select.h"
#include "commands/cmd_pipe_control.h"
#include "commands/cmd_push_constant_alloc.h"
#include "commands/cmd_render_surface_state.h"
#include "commands/cmd_scissor.h"
#include "commands/cmd_state_base_address.h"
#include "commands/cmd_vertex_buffers.h"
#include "commands/cmd_vertex_elements.h"
#include "commands/cmd_viewport.h"
#include "drivers/mmio_post_write.h"
#include "gpu/intel/bcs/blt_commands.h"
#include "gpu/intel/core/mi_commands.h"
#include "gpu/intel/core/mocs_init.h"
#include "klib/string.h"
#include "state/blend_state.h"
#include "state/primitive_topology.h"
#include "state/surface_format.h"
#include "vespera/time.h"
#include "vespera/mm/memory.h"

struct ShaderOffsets {
    u32 vs_offset;
    u32 ps_offset;
};

namespace {
    // Software-assigned Context ID for RCS's single execlist context (CONTEXT_DESCRIPTOR::sw_context_id,
    // 11 bits, up to 2048 per VF — see execlist_regs.h). Only one RCS context exists right now, so any
    // small nonzero value works; bump this into a proper per-context allocator once RCS supports more
    // than one context.
    constexpr u32 RCS_SW_CONTEXT_ID = 1;
}


namespace gpu::intel::rcs {
    IntelRcs::IntelRcs(core::IntelGpuDevice& device)
        : IntelEngine(
            EngineType::RCS, device, RCS_ENGINE_OFFSET,
            core::ForceWakeDomain{
                FORCEWAKE_RENDER, FORCEWAKE_ACK_RENDER, FORCEWAKE_RENDER_ENABLE, core::FORCEWAKE_ACK_BIT,
                FORCEWAKE_RENDER_TIMEOUT
            }
        ) {
    }

    bool IntelRcs::init_device(Resolution res) {
        if (!engine_force_wake_enable()) {
            Log::info("intel-rcs: ForceWake timeout");
            return false;
        }

        if (!engine_reset()) return false;
        Log::debug("RCS reset successful");

        core::MocsTable(device()).init();

        hwsp_alloc();
        ring_alloc_and_init(RCS_RING_BUFFER_SIZE);

        HWSTAM_REG stam{};
        stam.raw = 0xFFFFFFFFu;
        engine_reg_write(core::ENGINE_HWSTAM_OFF, stam);

        completion_flag_.init(false);

        rcs_interrupts_enable();

        if (!device().register_engine_for_irq(this)) {
            Log::info("intel-rcs: failed to register for GT interrupts");
            return false;
        }

       set_submission_mode(core::SubmissionMode::Execlist);

       Log::info("intel-rcs: ring + HWSP + LRC initialized (Execlist mode)");

        rcs_error_reporting_init();

        /*if (!select_pipeline(PIPELINE_SELECT::PIPELINE_3D)) {
        //    log_lrc_context_image();
            Log::info("intel-rcs: pipeline select failed");
            return false;
        }*/

        return true;

        // LEGACY SETUP ↓

        if (!state_base_address_setup()) {
            Log::info("intel-rcs: STATE_BASE_ADDRESS setup failed");
            return false;
        }

        setup_scissor_state(res);

        if (!setup_viewport_state(0.0f, 0.0f, res)) {
            Log::info("intel-rcs: setup viewport state failed");
            return false;
        }

        setup_blend_state();

        if (!vertex_buffer_setup()) {
            Log::log_dbc("intel-rcs: vertex buffer setup failed");
            return false;
        }

        ShaderOffsets offsets = upload_shaders();
        Log::debug("offsets: %u %u", offsets.ps_offset, offsets.vs_offset);

        if (!setup_shaders_and_pipeline(offsets)) {
            Log::info("intel-rcs: VS/PS setup failed");
            return false;
        }

        if (!setup_constant_buffer_allocations()) {
            Log::info("intel-rcs: constant buffer allocations failed");
            return false;
        }

        auto matrix = Mat4::identity();

        if (!setup_constant_buffer(matrix)) {
            Log::log_dbc("intel-rcs: constant buffer setup failed");
            return false;
        }

        if (!render_target_setup(res)) {
            Log::info("intel-rcs: render target setup failed");
            return false;
        }

        debug_dump_error_regs("baseline, before draw");
        debug_dump_render_target(res.width, res.height, res.width * 4);

        if (!draw_triangle()) {
            Log::info("intel-rcs: draw triangle failed");
            debug_dump_error_regs("after failed draw_triangle");
            dump_pipeline_stats("after draw fail");
            return false;
        }

        debug_dump_error_regs("after draw_triangle");
        debug_dump_render_target(res.width, res.height, res.width * 4);

        dump_pipeline_stats("after draw");

        return true;
    }

    void IntelRcs::rcs_interrupts_enable() const {
        volatile RCS_IMR_REG& imr = *reinterpret_cast<volatile RCS_IMR_REG*>(engine_regs() + ENGINE_IMR_OFF);

        RCS_IMR_REG rcs_imr{};
        rcs_imr.bits.user_irq = 0;
        rcs_imr.bits.pipe_control_notify = 0;
        rcs_imr.bits.master_error = 0;
        rcs_imr.bits.timeout = 0;
        rcs_imr.bits.page_fault = 0;
        rcs_imr.bits.ctx_switch = 0;
        rcs_imr.bits.invalid_tile = 0;
        rcs_imr.bits.l3_counter = 0;
        rcs_imr.bits.wait_sem = 0;
        imr.raw = rcs_imr.raw;
        MMIO_POST_WRITE(rcs_imr);
    }

    void IntelRcs::rcs_error_reporting_init() const {
        volatile auto* eir = reinterpret_cast<volatile EIR_REG*>(engine_regs() + EIR_OFFSET);
        volatile auto* emr = reinterpret_cast<volatile EMR_REG*>(engine_regs() + EMR_OFFSET);

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

    u32 IntelRcs::gt_user_irq_bit() const {
        return GT0_RCS_PIPE_CONTROL_NOTIFY_BIT;
    }

    u32 IntelRcs::gt_debug_irq_bitmask() const {
        constexpr u32 RCS_PAGE_FAULT_BIT = 7;
        constexpr u32 RCS_MASTER_ERROR_BIT = 3;
        return (1u << RCS_PAGE_FAULT_BIT) | (1u << RCS_MASTER_ERROR_BIT);
    }

    void IntelRcs::on_gt_user_interrupt() {
        Log::debug("on_gt_user_interrupt");
        completion_flag_.set();
    }

    void IntelRcs::emit_flush(u32 seqno) {
        PIPE_CONTROL flush_cmd = PIPE_CONTROL::create();
        flush_cmd.render_target_cache_flush_enable = 1;
        flush_cmd.depth_cache_flush_enable = 1;
        flush_cmd.dc_flush_enable = 1;
        ring_write_cmd(flush_cmd);

        PIPE_CONTROL seqno_cmd = PIPE_CONTROL::create();
        seqno_cmd.pipe_control_flush_enable = 1;
        seqno_cmd.command_streamer_stall_enable = 1;
        seqno_cmd.post_sync_operation = PIPE_CONTROL::WRITE_IMMEDIATE_DATA;
        seqno_cmd.destination_address_type = 0;
        seqno_cmd.store_data_index = 1;
        seqno_cmd.address_lo = core::PPHWSP_SEQNO_DWORD_INDEX;
        seqno_cmd.address_hi = 0;
        seqno_cmd.immediate_data = seqno;
        seqno_cmd.notify_enable = 1;
        ring_write_cmd(seqno_cmd);
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

    bool IntelRcs::present_to_screen(Resolution res) const {
        if (!bcs_) return false;
        Log::log_dbc("presenting to screen");

        const u32 pitch = res.width * 4;
        bcs_->composite_gpu_surface(render_target_gfx_addr_, pitch, res.width, res.height);
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
