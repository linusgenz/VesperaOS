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
#include "gfx_pipeline_stats_regs.h"
#include "commands/cmd_pipe_control.h"
#include "drivers/mmio_post_write.h"
#include "gpu/intel/bcs/blt_commands.h"
#include "gpu/intel/core/mi_commands.h"
#include "gpu/intel/core/mocs_init.h"

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

    u32 IntelRcs::gt_unmask_bits() const {
        return (1u << GT0_RCS_PIPE_CONTROL_NOTIFY_BIT) | (1u << GT0_RCS_CTX_SWTICH_BIT);
    }

    u32 IntelRcs::gt_debug_irq_bitmask() const {
        constexpr u32 RCS_PAGE_FAULT_BIT = 7;
        constexpr u32 RCS_MASTER_ERROR_BIT = 3;
        return (1u << RCS_PAGE_FAULT_BIT) | (1u << RCS_MASTER_ERROR_BIT);
    }

    void IntelRcs::on_gt_user_interrupt() {
        completion_flag_.set();
        waiters.wake_all_irq();
    }

    void IntelRcs::emit_flush(u32 seqno) {
        PIPE_CONTROL flush_pc = PIPE_CONTROL::create();
        flush_pc.render_target_cache_flush_enable = 1;
        flush_pc.depth_cache_flush_enable         = 1;
        flush_pc.dc_flush_enable                  = 1;
        flush_pc.tlb_invalidate                   = 1;
        flush_pc.command_streamer_stall_enable    = 1;
        ring_write_cmd(flush_pc);

        PIPE_CONTROL seq_pc = PIPE_CONTROL::create();
        seq_pc.pipe_control_flush_enable     = 1;
        seq_pc.command_streamer_stall_enable = 1;
        seq_pc.post_sync_operation           = PIPE_CONTROL::WRITE_IMMEDIATE_DATA;
        seq_pc.destination_address_type      = 0;
        seq_pc.store_data_index              = 1;
        seq_pc.address_lo                    = core::PPHWSP_SEQNO_DWORD_INDEX;
        seq_pc.address_hi                    = 0;
        seq_pc.immediate_data                = seqno;
        seq_pc.notify_enable                 = 1;
        ring_write_cmd(seq_pc);
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
