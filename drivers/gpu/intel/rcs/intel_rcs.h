// intel_rcs.h
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
#ifndef VESPERAOS_INTEL_RCS_H
#define VESPERAOS_INTEL_RCS_H

#include "commands/cmd_pipeline_select.h"
#include <gpu/intel/core/intel_engine.h>
#include <gpu/intel/bcs/intel_bcs.h>
#include <vespera/graphics/display_types.h>

struct Mat4;
struct ShaderOffsets;

namespace gpu::intel::rcs {
    // RCS MMIO range: 0x2000-0x27FF (PRM Vol 6, "Render Engine Command
    // Streamer (RCS)"). Passed as this engine's MMIO offset to IntelEngine.
    constexpr u32 RCS_ENGINE_OFFSET = 0x2000;

    // ForceWake — Render domain (Gen9 splits ForceWake per engine; this is a
    // *different* request/ack register pair than BCS's FORCEWAKE_BLITTER in
    // intel_bcs.h — confirmed against the public Gen9 register layout:
    // FORCEWAKE_RENDER_GEN9 / FORCEWAKE_ACK_RENDER_GEN9).
    constexpr u32 FORCEWAKE_RENDER = 0xA278;
    constexpr u32 FORCEWAKE_ACK_RENDER = 0x0D84;
    constexpr u32 FORCEWAKE_RENDER_ENABLE = 0x00010001;
    constexpr u32 FORCEWAKE_RENDER_TIMEOUT = 1000;

    constexpr u32 RCS_RING_BUFFER_SIZE = 16u * 1024u;

    /**
     * @brief Render Command Streamer engine.
     *
     * Peer of IntelBcs — both inherit IntelEngine and borrow the same
     * IntelGpuDevice for MMIO base and GGTT
     */
    class IntelRcs final : public core::IntelEngine {
    public:
        explicit IntelRcs(core::IntelGpuDevice& device);

        IntelRcs(const IntelRcs&) = delete;
        IntelRcs& operator=(const IntelRcs&) = delete;

        bool init_device();

        void dump_pipeline_stats(const char* label) const;
        void debug_dump_error_regs(const char* label) const;
    private:
        bool select_pipeline(PIPELINE_SELECT::PipelineSelection mode);
        u32 gt_user_irq_bit() const override;
        u32 gt_unmask_bits() const override;
        u32 gt_debug_irq_bitmask() const override;
        void on_gt_user_interrupt() override;
        void rcs_interrupts_enable() const;
        void emit_flush(u32 seqno) override;

        void rcs_error_reporting_init() const;

        AtomicFlag completion_flag_{};
    };
} // namespace blt

#endif  // VESPERAOS_INTEL_RCS_H
