// hpet.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.04.26.
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

#ifndef VESPERAOS_KERNEL_TIME_HPET_H
#define VESPERAOS_KERNEL_TIME_HPET_H

#include <vespera/types.h>

#include "clock_source.h"

namespace kernel::time {

    // HPET MMIO register offsets  (ACPI 5.0 §9.14)
    constexpr u32 HPET_REG_GCAP_ID = 0x000;    // General Capabilities and ID (64-bit)
    constexpr u32 HPET_REG_GEN_CONF = 0x010;   // General Configuration (64-bit)
    constexpr u32 HPET_REG_GINTR_STA = 0x020;  // General Interrupt Status (64-bit)
    constexpr u32 HPET_REG_MAIN_CNT = 0x0F0;   // Main Counter Value (64-bit)

    // GCAP_ID field layout
    constexpr u32 HPET_GCAP_PERIOD_SHIFT = 32;          // counter period [63:32], in femtoseconds
    constexpr u64 HPET_GCAP_COUNT_SIZE = (1ULL << 13);  // 1 → 64-bit counter
    constexpr u64 HPET_GCAP_LEG_RT_CAP = (1ULL << 15);  // legacy replacement IRQ routing capable

    // GEN_CONF bits
    constexpr u64 HPET_CONF_ENABLE = (1ULL << 0);  // start main counter
    constexpr u64 HPET_CONF_LEG_RT = (1ULL << 1);  // enable legacy IRQ routing (replaces PIT+RTC)

    // Sanity bounds on the counter period (ACPI spec Table 9-107)
    constexpr u64 HPET_MIN_PERIOD_FS = 100'000ULL;              // 100 ns minimum
    constexpr u64 HPET_MAX_PERIOD_FS = 100'000'000'000'000ULL;  // 100 ms maximum

    class HpetClock final : public IClockSource {
       public:
        HpetClock() = default;
        ~HpetClock() override = default;

        HpetClock(const HpetClock&) = delete;
        HpetClock& operator=(const HpetClock&) = delete;

        [[nodiscard]] const char* name() const override {
            return "HPET";
        }
        [[nodiscard]] int init() override;
        [[nodiscard]] bool available() const override {
            return available_;
        }
        [[nodiscard]] clock_priority priority() const override {
            return clock_priority::HPET;
        }
        [[nodiscard]] u64 read_ticks() override;
        [[nodiscard]] u64 frequency_hz() const override {
            return freq_hz_;
        }
        [[nodiscard]] u64 read_ns() override;

       private:
        [[nodiscard]] u64 reg_read64(u32 offset) const;
        void reg_write64(u32 offset, u64 value) const;

        volatile u8* mmio_base_ = nullptr;
        u64 freq_hz_ = 0;
        u64 period_fs_ = 0;
        bool available_ = false;
        bool is_64_bit_ = false;
    };

}  // namespace kernel::time

#endif  // VESPERAOS_KERNEL_TIME_HPET_H
