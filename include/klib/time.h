// time.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.03.26.
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
#ifndef VESPERAOS_KLIB_TIME_H
#define VESPERAOS_KLIB_TIME_H

#include <vespera/types.h>

namespace klib::time {

    /**
     * @brief Check if a year is a leap year.
     */
    constexpr bool is_leap_year(u32 year) {
        return (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0);
    }

    /**
     * @brief Convert a broken-down date/time to a Unix timestamp (seconds since 1970-01-01 00:00:00 UTC).
     *
     * @param year   Full year (e.g. 2026)
     * @param month  Month [1..12]
     * @param day    Day of month [1..31]
     * @param hour   Hour [0..23]
     * @param minute Minute [0..59]
     * @param sec    Second [0..60]
     * @return Unix timestamp as u64
     */
    u64 to_unix(u32 year, u8 month, u8 day, u8 hour, u8 minute, u8 sec);

}  // namespace klib::time

#endif  // VESPERAOS_KLIB_TIME_H
