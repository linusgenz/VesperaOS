// time.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.03.26.
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
#ifndef VESPERAOS_UAPI_TIME_H
#define VESPERAOS_UAPI_TIME_H

#include <vespera/types.h>

/**
 * @brief Represents a point in time with nanosecond precision.
 */
typedef struct timespec {
    i64 tv_sec;   ///< Seconds
    i64 tv_nsec;  ///< Nanoseconds (0–999999999)
} timespec_t;

/**
 * @brief Clock source identifiers for sys_clock_gettime.
 */
typedef i32 clockid_t;

#define CLOCK_REALTIME           0  ///< Wall-clock time (from RTC)
#define CLOCK_MONOTONIC          1  ///< Monotonic uptime; never jumps backwards
#define CLOCK_MONOTONIC_RAW      4  ///< Like CLOCK_MONOTONIC, not adjusted


#endif  // VESPERAOS_UAPI_TIME_H
