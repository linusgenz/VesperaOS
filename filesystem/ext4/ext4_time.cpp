// ext4_time.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.03.26.
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

#include <vespera/types.h>
#include <vespera/time.h>

namespace ext4 {
    u64 rtc_to_unix_time() {
        u8 sec, min, hour, day, month, year;
        kernel::time::read_rtc(sec, min, hour, day, month, year);

        // RTC just delivers year since 2000 so we have to add 2000 here
        const u16 full_year = 2000 + year;

        // Days since 01/01/1970
        auto is_leap = [](u16 y) { return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)); };

        u64 days = 0;
        for (u16 y = 1970; y < full_year; ++y) {
            days += is_leap(y) ? 366 : 365;
        }

        static const u8 month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
        for (u8 m = 1; m < month; ++m) {
            days += month_days[m-1];
            if (m == 2 && is_leap(full_year)) days += 1;
        }

        days += (day - 1);

        u64 total_seconds = days * 86400 + hour * 3600 + min * 60 + sec;
        return total_seconds;
    }
}