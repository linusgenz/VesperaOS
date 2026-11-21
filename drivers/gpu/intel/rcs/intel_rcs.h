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

#include <gpu/intel/core/intel_engine.h>
#include <gpu/intel/bcs/intel_bcs.h>

namespace gpu::intel::rcs {
    // RCS MMIO range: 0x2000-0x27FF (PRM Vol 6, "Render Engine Command
    // Streamer (RCS)"). Passed as this engine's MMIO offset to IntelEngine.
    inline constexpr u32 RCS_ENGINE_OFFSET = 0x2000;

    inline constexpr u32 RCS_RING_BUFFER_SIZE = 16u * 1024u;

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
        u32 gt_user_irq_bit() const override;
        u32 gt_unmask_bits() const override;
        u32 gt_debug_irq_bitmask() const override;
        void on_gt_user_interrupt() override;
        void rcs_interrupts_enable() const;
        void emit_flush(u32 seqno) override;

        [[nodiscard]] usize lrc_size_bytes() const override { return core::LRC_SIZE_RCS; }

        void rcs_error_reporting_init() const;

        AtomicFlag completion_flag_{};
    };
} // namespace gpu::intel::rcs

#endif  // VESPERAOS_INTEL_RCS_H
