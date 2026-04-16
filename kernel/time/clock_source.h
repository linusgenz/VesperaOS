// clock_source.h
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

#ifndef VESPERAOS_KERNEL_TIME_CLOCK_SOURCE_H
#define VESPERAOS_KERNEL_TIME_CLOCK_SOURCE_H

#include <vespera/types.h>

namespace kernel::time {

    enum class clock_priority : u32 {
        INVALID = 0,
        PIT     = 10,   // 8254 PIT, ~1.19 MHz, interrupt-driven
        APIC    = 20,   // per-CPU APIC timer
        HPET    = 30,   // ACPI HPET, high-resolution hardware counter
        TSC     = 40,   // invariant TSC
    };

    // Abstract base for every clock source
    class IClockSource {
    public:
        virtual ~IClockSource() = default;

        IClockSource(const IClockSource&)            = delete;
        IClockSource& operator=(const IClockSource&) = delete;

        [[nodiscard]] virtual const char* name() const = 0;

        [[nodiscard]] virtual int init() = 0;

        [[nodiscard]] virtual bool available() const = 0;

        [[nodiscard]] virtual clock_priority priority() const = 0;

        // Raw hardware counter value (source-specific unit, always monotonic).
        [[nodiscard]] virtual u64 read_ticks() = 0;

        // Nominal tick frequency in Hz.
        [[nodiscard]] virtual u64 frequency_hz() const = 0;

        // Nanoseconds elapsed since this source was initialized.
        [[nodiscard]] virtual u64 read_ns() = 0;

    protected:
        IClockSource() = default;
    };

} // namespace kernel::time

#endif // VESPERAOS_KERNEL_TIME_CLOCK_SOURCE_H