// sys_clock_gettime.cpp
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

#include <uapi/vespera/time.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/time.h>
#include <vespera/types.h>

#include "klib/time.h"

namespace syscalls::internal {

    static i64 fill_realtime(timespec_t* ts) {
        u8 sec = 0, min = 0, hour = 0, day = 0, month = 0, year = 0;
        kernel::time::read_rtc(sec, min, hour, day, month, year);

        ts->tv_sec  = static_cast<i64>(klib::time::to_unix(2000u + year, month, day, hour, min, sec));
        ts->tv_nsec = 0;  // fill this field when we have more precise timer
        return SUCCESS_CODE;
    }

    static i64 fill_monotonic(timespec_t* ts) {
        const u64 uptime_ms = kernel::time::get_uptime_ms();
        ts->tv_sec = static_cast<i64>(uptime_ms / 1000);
        ts->tv_nsec = static_cast<i64>((uptime_ms % 1000) * 1'000'000LL);
        return SUCCESS_CODE;
    }

    i64 sys_clock_gettime(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto clk_id = static_cast<clockid_t>(arg0);
        auto* ts = reinterpret_cast<timespec_t*>(arg1);

        if (!ts) return -EINVAL;

        switch (clk_id) {
            case CLOCK_REALTIME:
                return fill_realtime(ts);

            case CLOCK_MONOTONIC:
            case CLOCK_MONOTONIC_RAW:
                return fill_monotonic(ts);

            default:
                return -EINVAL;
        }
    }
}  // namespace syscalls::internal