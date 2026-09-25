// sys_eventfd2.cpp
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

#include <vespera/sync/spinlock.h>
#include <vespera/ipc/eventfd.h>
#include <vespera_errno.h>

#include <uapi/vespera/eventfd.h>

#include "realm/realm.h"
#include "vespera/scheduling.h"
#include "vespera/realm/handles.h"

class Realm;

namespace syscalls::internal {
    i64 sys_eventfd2(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const u64 initval = arg0;
        const u32 flags = static_cast<u32>(arg1);

        constexpr u32 valid_flags = EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE;
        if (flags & ~valid_flags) return -EINVAL;

        const Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        auto* efd = Eventfd::create(initval, flags & EFD_SEMAPHORE, flags & EFD_NONBLOCK);
        if (!efd) return -ENOMEM;

        auto hdl_result = kernel::realm::add_handle_to_current(HANDLE_TYPE_EVENTFD, efd, CAP_READ | CAP_WRITE, true, Eventfd::destroy, Eventfd::ref);
        if (hdl_result.is_err()) {
            Eventfd::destroy(efd);
            return -EMFILE;
        }

        return static_cast<i64>(hdl_result.unwrap());
    }
} // namespace syscalls::internal