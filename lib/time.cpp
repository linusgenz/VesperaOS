// time.cpp
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

#include <klib/time.h>

namespace klib::time {

    static const u8 MONTH_DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    u64 to_unix(const u32 year, const u8 month, const u8 day, const u8 hour, const u8 minute, const u8 sec) {
        u64 days = 0;

        for (u32 y = 1970; y < year; ++y) days += is_leap_year(y) ? 366 : 365;

        for (u8 m = 1; m < month; ++m) {
            days += MONTH_DAYS[m - 1];
            if (m == 2 && is_leap_year(year)) days += 1;
        }

        days += day - 1;

        return days * 86400ULL + hour * 3600ULL + minute * 60ULL + sec;
    }

}  // namespace klib::time