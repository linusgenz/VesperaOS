// sys_gettimeofday.cpp
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

namespace syscalls::internal {
    i64 sys_gettimeofday(u64 arg0, u64, u64, u64, u64, u64) {
        auto* tv = reinterpret_cast<timeval_t*>(arg0);
        if (!tv) return 0;

        // TODO timezones

        const u64 ns    = kernel::time::get_realtime_ns();
        tv->tv_sec      = static_cast<i64>(ns / 1'000'000'000ULL);
        tv->tv_usec     = static_cast<i64>((ns % 1'000'000'000ULL) / 1'000ULL);
        return 0;
    }
}  // namespace syscalls::internal
