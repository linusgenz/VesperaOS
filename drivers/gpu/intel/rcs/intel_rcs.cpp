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
#include "gpu/intel/core/intel_forcewake.h"
#include "gpu/intel/core/mocs_init.h"
#include "gpu/intel/regs/error_regs.h"

namespace gpu::intel::rcs {
    IntelRcs::IntelRcs(core::IntelGpuDevice& device)
        : IntelEngine(
            EngineType::RCS, device, RCS_ENGINE_OFFSET,
            core::RENDER_FORCEWAKE_DOMAIN
        ) {
    }

    bool IntelRcs::init_device() {
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
            Log::warning("intel-rcs: HARDWARE ERROR DETECTED (EIR or ESR non-zero) - see values above");
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
