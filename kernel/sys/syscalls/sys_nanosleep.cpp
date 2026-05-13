// sys_sleep.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 13.08.25.
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

#include "vespera_errno.h"

namespace syscalls::internal {
    i64 sys_nanosleep(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto* req = reinterpret_cast<const timespec_t*>(arg0);
        auto* rem = reinterpret_cast<timespec_t*>(arg1);

        const u64 sleep_ns_val = static_cast<u64>(req->tv_sec) * 1'000'000'000ULL + static_cast<u64>(req->tv_nsec);

        const u64 deadline_ns = kernel::time::get_uptime_ns() + sleep_ns_val;

        const bool completed = kernel::time::sleep_ns(sleep_ns_val);

        if (rem) {
            const u64 now  = kernel::time::get_uptime_ns();
            const u64 left = (now < deadline_ns) ? (deadline_ns - now) : 0ULL;
            rem->tv_sec    = static_cast<i64>(left / 1'000'000'000ULL);
            rem->tv_nsec   = static_cast<i64>(left % 1'000'000'000ULL);
        }

        return completed ? 0 : -EINTR;
    }
}  // namespace syscalls::internal
