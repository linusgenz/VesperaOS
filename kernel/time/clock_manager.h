// clock_manager.h
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

#ifndef VESPERAOS_KERNEL_TIME_CLOCK_MANAGER_H
#define VESPERAOS_KERNEL_TIME_CLOCK_MANAGER_H

#include <vespera/types.h>

#include "clock_source.h"

namespace kernel::time::clock_manager {

    void init();

    [[nodiscard]] const char* active_source_name();

    [[nodiscard]] IClockSource* active_source();

    // Raw ticks from the active clock source.
    [[nodiscard]] u64 read_ticks();

    // Nanoseconds since boot (monotonic).
    [[nodiscard]] u64 read_ns();

    // Microseconds since boot.
    [[nodiscard]] u64 read_us();

    // Milliseconds since boot.
    [[nodiscard]] u64 read_ms();

}  // namespace kernel::time::clock_manager

#endif  // VESPERAOS_KERNEL_TIME_CLOCK_MANAGER_H