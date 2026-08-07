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
#include "gfx_pipeline_regs.h"
#include "cmd_state_base_address.h"
#include "cmd_pipe_control.h"

#include <vespera/log.h>

#include "drivers/mmio_post_write.h"
#include "gpu/intel/gt_interrupt_regs.h"
#include "gpu/intel/intel_bcs.h"
#include "klib/string.h"
#include "vespera/mm/memory.h"

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

        if (!select_pipeline(PIPELINE_SELECT::PIPELINE_3D)) {
            Log::info("intel-rcs: pipeline select failed");
            return false;
        }

        if (!state_base_address_setup()) {
            Log::info("intel-rcs: STATE_BASE_ADDRESS setup failed");
            return false;
        }

        Log::info("intel-rcs: pipeline selected (3D) + STATE_BASE_ADDRESS programmed (Milestone 2, steps 1-2)");

        return true;
    }

    void IntelRcs::rcs_interrupts_enable() const {
        volatile RCS_IMR_REG& imr = *reinterpret_cast<volatile RCS_IMR_REG*>(engine_regs() + RCS_IMR_OFF);

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

    u32 IntelRcs::gt_user_irq_bit() const {
        return GT0_RCS_PIPE_CONTROL_NOTIFY_BIT;
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
} // namespace blt
