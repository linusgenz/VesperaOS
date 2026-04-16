// apic_timer.h
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

#ifndef VESPERAOS_KERNEL_TIME_APIC_CLOCK_H
#define VESPERAOS_KERNEL_TIME_APIC_CLOCK_H

#include <vespera/types.h>

#include "../../arch/x86_64/interrupts/apic.h"
#include "clock_source.h"

namespace kernel::time {

    class ApicClock final : public IClockSource {
       public:
        ApicClock() = default;
        ~ApicClock() override = default;

        ApicClock(const ApicClock&) = delete;
        ApicClock& operator=(const ApicClock&) = delete;

        [[nodiscard]] const char* name() const override {
            return "APIC";
        }
        [[nodiscard]] int init() override;
        [[nodiscard]] bool available() const override {
            return available_;
        }
        [[nodiscard]] clock_priority priority() const override {
            return clock_priority::APIC;
        }
        [[nodiscard]] u64 read_ticks() override;
        [[nodiscard]] u64 frequency_hz() const override {
            return arch::x86_64::interrupts::apic::APIC_TICK_HZ;
        }
        [[nodiscard]] u64 read_ns() override;

       private:
        bool available_ = false;
    };

}  // namespace kernel::time

#endif  // VESPERAOS_KERNEL_TIME_APIC_CLOCK_H