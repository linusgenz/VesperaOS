// intel_engine.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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
#ifndef VESPERAOS_INTEL_ENGINE_H
#define VESPERAOS_INTEL_ENGINE_H

#include <vespera/mm/addr.h>
#include <vespera/sync/atomic.h>
#include <vespera/types.h>

#include "bcs_regs.h"
#include "intel_engine_types.h"
#include "intel_gpu_device.h"


namespace blt {
    constexpr u32 ENGINE_RING_TAIL_OFF = 0x30;
    constexpr u32 ENGINE_RING_HEAD_OFF = 0x34;
    constexpr u32 ENGINE_RING_START_OFF = 0x38;
    constexpr u32 ENGINE_RING_CTL_OFF = 0x3C;
    constexpr u32 ENGINE_HWS_PGA_OFF = 0x80;
    constexpr u32 ENGINE_HWSTAM_OFF = 0x98;

    constexpr u32 HWSP_SEQNO_OFFSET_DWORDS = 4;

    /**
     * @brief Common ring-buffer/HWSP/seqno machinery shared by every Intel
     *        command-streamer engine.
     *
     * Does NOT own the device — every engine borrows the same
     * IntelGpuDevice for MMIO base and GGTT.
     */
    class IntelEngine {
    public:
        IntelEngine(EngineType type, IntelGpuDevice& device, u32 engine_mmio_offset, ForceWakeDomain fw_domain);
        virtual ~IntelEngine() = default;

        IntelEngine(const IntelEngine&) = delete;
        IntelEngine& operator=(const IntelEngine&) = delete;

        /// Wakes this engine's ForceWake domain (Render for RCS, Blitter for
        /// BCS, ...). Must succeed before touching any of this engine's
        /// MMIO registers.
        [[nodiscard]] bool engine_force_wake_enable() const {
            return device_.force_wake_enable(fw_domain_);
        }

        /// This engine's bit position within the shared GT0 interrupt
        /// register group (GT0_ISR/IMR/IIR/IER, MMIO 0x44300).
        [[nodiscard]] virtual u32 gt_user_irq_bit() const = 0;

        /// Called by IntelGpuDevice's shared IRQ dispatcher when this
        /// engine's bit is set in GT0_IIR.
        virtual void on_gt_user_interrupt() {
        }

        [[nodiscard]] virtual u32 gt_debug_irq_bitmask() const {
            return 0;
        }


    protected:
        EngineType type_;

        [[nodiscard]] IntelGpuDevice& device() const {
            return device_;
        }

        [[nodiscard]] GgttAllocator& ggtt() const {
            return device_.ggtt();
        }

        [[nodiscard]] bool engine_reset(u32 timeout_us = 10000) const;

        [[nodiscard]] volatile u8* engine_regs() const {
            return device_.mmio_base() + engine_mmio_offset_;
        }

        template <class T>
        [[nodiscard]] T engine_reg_read(u32 offset) const {
            T val;
            val.raw = *reinterpret_cast<volatile u32*>(engine_regs() + offset);
            return val;
        }

        template <class T>
        void engine_reg_write(u32 offset, T val) const {
            *reinterpret_cast<volatile u32*>(engine_regs() + offset) = val.raw;
        }

        template <typename T>
        [[nodiscard]] T mmio_read(u32 reg) const {
            T val;
            val.raw = *reinterpret_cast<volatile u32*>(device_.mmio_base() + reg);
            return val;
        }

        template <typename T>
        void mmio_write(u32 reg, T val) const {
            *reinterpret_cast<volatile u32*>(device_.mmio_base() + reg) = val.raw;
        }

        /// Allocates the ring in GGTT, zero-fills it with MI_NOOP, and
        /// programs RING_BUFFER_START/CTL/HEAD/TAIL + masks HWSTAM.
        void ring_alloc_and_init(u32 ring_size_bytes);

        void ring_write(u32 dword);

        template <typename T>
        void ring_write_cmd(const T& cmd) {
            static_assert(sizeof(T) % sizeof(u32) == 0, "Command size must be DWORD-aligned");

            const auto* dwords = reinterpret_cast<const u32*>(&cmd);
            const usize count = sizeof(T) / sizeof(u32);
            for (usize i = 0; i < count; i++) {
                ring_write(dwords[i]);
            }
        }

        void ring_flush();
        [[nodiscard]] bool ring_wait_space(u32 required_bytes, u32 timeout_us) const;

        void hwsp_alloc();
        u32 seqno_next();
        bool seqno_wait(u32 target_seqno, u32 timeout_us, AtomicFlag& completion_flag);

        gfx_addr_t ring_gfx_addr_{};
        virt_addr_t ring_cpu_addr_{};
        u32 ring_size_ = 0;
        u32 ring_tail_ = 0;

        gfx_addr_t hwsp_gfx_addr_{};
        virt_addr_t hwsp_cpu_addr_{};
        u32 sequence_number_ = 0;

        u64 error_count_ = 0;

        static constexpr u32 SEQNO_BIT5_MASK = 1u << 5;

    private:
        IntelGpuDevice& device_;
        u32 engine_mmio_offset_;
        ForceWakeDomain fw_domain_;
    };
} // namespace blt

#endif  // VESPERAOS_INTEL_ENGINE_H
