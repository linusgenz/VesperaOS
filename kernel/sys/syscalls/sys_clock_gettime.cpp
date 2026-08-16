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

#include "vespera/scheduling.h"

namespace syscalls::internal {
    static i64 fill_realtime(timespec_t* ts) {
        const u64 realtime_ns = kernel::time::get_realtime_ns();
        ts->tv_sec = static_cast<i64>(realtime_ns / 1'000'000'000ULL);
        ts->tv_nsec = static_cast<i64>(realtime_ns % 1'000'000'000ULL);
        return SUCCESS_CODE;
    }

    static i64 fill_monotonic(timespec_t* ts) {
        const u64 uptime_ms = kernel::time::get_uptime_ms();
        ts->tv_sec = static_cast<i64>(uptime_ms / 1000);
        ts->tv_nsec = static_cast<i64>((uptime_ms % 1000) * 1'000'000LL);
        return SUCCESS_CODE;
    }

    static i64 fill_process_cputime(timespec_t* ts) {
        const RealmId rid = kernel::scheduling::get_current_realm_id();
        if (rid == 0) return -EINVAL;

        const u64 total_ns = kernel::scheduling::get_realm_cpu_time_ns(rid);

        ts->tv_sec = static_cast<i64>(total_ns / 1'000'000'000ULL);
        ts->tv_nsec = static_cast<i64>(total_ns % 1'000'000'000ULL);
        return SUCCESS_CODE;
    }

    static i64 fill_thread_cputime(clockid_t clk_id, timespec_t* ts) {
        const u64 decoded = ~static_cast<u64>(clk_id);
        const UnitId unit_id = decoded >> 3;

        const u64 total_ns = kernel::scheduling::get_unit_cpu_time_ns(unit_id);
        if (total_ns == 0) return -ESRCH;

        ts->tv_sec = static_cast<i64>(total_ns / 1'000'000'000ULL);
        ts->tv_nsec = static_cast<i64>(total_ns % 1'000'000'000ULL);
        return SUCCESS_CODE;
    }


    i64 sys_clock_gettime(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto clk_id = static_cast<clockid_t>(arg0);
        auto* ts = reinterpret_cast<timespec_t*>(arg1);
        if (!ts) return -EINVAL;

        if (clk_id < 0 && (static_cast<u64>(~clk_id) & 0x7) == CPUCLOCK_PERTHREAD_MASK) {
            return fill_thread_cputime(clk_id, ts);
        }

        switch (clk_id) {
            case CLOCK_REALTIME:
                return fill_realtime(ts);

            case CLOCK_MONOTONIC:
            case CLOCK_MONOTONIC_RAW:
                return fill_monotonic(ts);
            case CLOCK_PROCESS_CPUTIME_ID:
                return fill_process_cputime(ts);
            case CLOCK_THREAD_CPUTIME_ID: {
                const UnitId self = kernel::scheduling::get_current_unit_id();
                const u64 total_ns = kernel::scheduling::get_unit_cpu_time_ns(self);
                ts->tv_sec = static_cast<i64>(total_ns / 1'000'000'000ULL);
                ts->tv_nsec = static_cast<i64>(total_ns % 1'000'000'000ULL);
                return SUCCESS_CODE;
            }
            default:
                return -EINVAL;
        }
    }
} // namespace syscalls::internal
