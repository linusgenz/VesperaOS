// apic_clock.cpp
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

#include "apic_clock.h"

#include "../arch/x86_64/interrupts/apic.h"
#include <vespera/log.h>

namespace kernel::time {

    int ApicClock::init() {
        available_ = true;
        Log::ok("[APIC] Clock source registered: %llu Hz", arch::x86_64::interrupts::apic::APIC_TICK_HZ);
        return 0;
    }

    u64 ApicClock::read_ticks() {
        return arch::x86_64::interrupts::apic::apic_ticks[0];
    }

    u64 ApicClock::read_ns() {
        return read_ticks() * (1'000'000'000ULL / arch::x86_64::interrupts::apic::APIC_TICK_HZ);
    }

} // namespace kernel::time