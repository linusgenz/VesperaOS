// sys_clock_nanosleep.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.04.26.
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
#include <vespera/time.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_clock_nanosleep(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto clk_id = static_cast<clockid_t>(arg0);
        const int flags = static_cast<int>(arg1);
        const auto* req = reinterpret_cast<const timespec_t*>(arg2);
        auto* rem = reinterpret_cast<timespec_t*>(arg3);

        if (clk_id != CLOCK_REALTIME && clk_id != CLOCK_MONOTONIC && clk_id != CLOCK_BOOTTIME) {
            return -EINVAL;
        }

        const u64 req_ns = static_cast<u64>(req->tv_sec) * 1'000'000'000ULL + static_cast<u64>(req->tv_nsec);

        u64 sleep_ns = 0;
        u64 deadline_abs = 0;  // absolute uptime deadline

        if (flags & TIMER_ABSTIME) {
            if (clk_id == CLOCK_REALTIME) {
                u64 now_clock = 0;
                now_clock = kernel::time::get_realtime_ns();
                deadline_abs = kernel::time::get_uptime_ns() + (req_ns > now_clock ? req_ns - now_clock : 0);
            } else {
                deadline_abs = req_ns;  // already in uptime space
            }
            sleep_ns =
                (deadline_abs > kernel::time::get_uptime_ns()) ? deadline_abs - kernel::time::get_uptime_ns() : 0;
        } else {
            sleep_ns = req_ns;
            deadline_abs = kernel::time::get_uptime_ns() + sleep_ns;
        }

        if (sleep_ns > 0) {
            kernel::time::sleep_ns(sleep_ns);
        }

        // TODO signal support here, if interrupted by SIG write remaining here
        if (rem && !(flags & TIMER_ABSTIME)) {
            const u64 now = kernel::time::get_uptime_ns();
            const u64 left = (now < deadline_abs) ? (deadline_abs - now) : 0ULL;
            rem->tv_sec = static_cast<i64>(left / 1'000'000'000ULL);
            rem->tv_nsec = static_cast<i64>(left % 1'000'000'000ULL);
        }

        return 0;
    }
}  // namespace syscalls::internal
