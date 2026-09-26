// sys_timerfd.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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

#include <uapi/vespera/timerfd.h>
#include <vespera_errno.h>
#include <vespera/time/timerfd.h>

#include <vespera/types.h>

#include "sys/handle_resolution.h"
#include "uapi/vespera/time.h"
#include "vespera/scheduling.h"
#include "vespera/realm/handles.h"

class Realm;

namespace syscalls::internal {
    i64 sys_timerfd_create(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const int clockid = static_cast<int>(arg0);
        const u32 flags = static_cast<u32>(arg1);

        constexpr u32 valid_flags = TFD_CLOEXEC | TFD_NONBLOCK;
        if (flags & ~valid_flags) return -EINVAL;
        if (clockid != CLOCK_MONOTONIC && clockid != CLOCK_REALTIME) return -EINVAL;

        const Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        auto* tfd = Timerfd::create(clockid, flags & TFD_NONBLOCK);
        if (!tfd) return -ENOMEM;

        auto hdl_result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_TIMERFD, tfd, CAP_READ, true, Timerfd::destroy, Timerfd::ref);
        if (hdl_result.is_err()) {
            Timerfd::destroy(tfd);
            return -EMFILE;
        }


        return static_cast<i64>(hdl_result.unwrap());
    }

    i64 sys_timerfd_settime(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto hdl = static_cast<HandleId>(arg0);
        const int flags = static_cast<int>(arg1);
        const auto* new_value = reinterpret_cast<const itimerspec*>(arg2);
        auto* old_value = reinterpret_cast<itimerspec*>(arg3);

        if (!new_value) return -EFAULT;

        const Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        auto rh_result = resolve_handle(hdl);
        if (rh_result.is_err()) return -EBADH;

        const auto& rh = rh_result.unwrap();
        if (rh.type() != HANDLE_TYPE_TIMERFD) return -EINVAL;

        auto* tfd = rh.resource_as<Timerfd>();
        if (!tfd) return -EINVAL;

        const bool abstime = flags & TFD_TIMER_ABSTIME;
        const u64 value_ns = new_value->it_value.tv_sec * 1'000'000'000ULL + new_value->it_value.tv_nsec;
        const u64 interval_ns = new_value->it_interval.tv_sec * 1'000'000'000ULL + new_value->it_interval.tv_nsec;

        u64 old_value_ns = 0, old_interval_ns = 0;
        const i64 ret = tfd->settime(flags, value_ns, interval_ns, abstime,
                                     old_value ? &old_value_ns : nullptr,
                                     old_value ? &old_interval_ns : nullptr);

        if (old_value) {
            old_value->it_value.tv_sec = static_cast<long>(old_value_ns / 1'000'000'000ULL);
            old_value->it_value.tv_nsec = static_cast<long>(old_value_ns % 1'000'000'000ULL);
            old_value->it_interval.tv_sec = static_cast<long>(old_interval_ns / 1'000'000'000ULL);
            old_value->it_interval.tv_nsec = static_cast<long>(old_interval_ns % 1'000'000'000ULL);
        }

        return ret;
    }
} // namespace syscalls::internal
