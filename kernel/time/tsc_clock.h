// tsc_clock.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.04.26.
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

#ifndef VESPERAOS_KERNEL_TIME_TSC_CLOCK_H
#define VESPERAOS_KERNEL_TIME_TSC_CLOCK_H

#include <vespera/types.h>

#include "clock_source.h"

namespace kernel::time {

    constexpr u32 TSC_CALIBRATE_US = 50'000;  // 50 ms

    class TscClock final : public IClockSource {
       public:
        TscClock() = default;
        ~TscClock() override = default;

        TscClock(const TscClock&) = delete;
        TscClock& operator=(const TscClock&) = delete;

        [[nodiscard]] const char* name() const override {
            return "TSC";
        }

        // Returns 0 on success, negative errno on failure.
        [[nodiscard]] int init() override;

        [[nodiscard]] bool available() const override {
            return available_;
        }
        [[nodiscard]] clock_priority priority() const override {
            return clock_priority::TSC;
        }

        // Raw RDTSC value (monotonic on invariant TSC hardware).
        [[nodiscard]] u64 read_ticks() override;

        [[nodiscard]] u64 frequency_hz() const override {
            return freq_hz_;
        }

        [[nodiscard]] u64 read_ns() override;

       private:
        // Returns true if the CPU advertises an invariant (non-stop) TSC.
        // Checks CPUID leaf 0x80000007, EDX bit 8.
        [[nodiscard]] static bool has_invariant_tsc();

        [[nodiscard]] static u64 rdtsc();
        static u64 measure_freq_once();

        u64 freq_hz_ = 0;
        u64 mult_ = 0;
        u32 shift_ = 0;
        u64 tsc_start_ = 0;
        bool available_ = false;
    };

}  // namespace kernel::time

#endif  // VESPERAOS_KERNEL_TIME_TSC_CLOCK_H