// sys_signalfd4.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.09.26.
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
#include <vespera/signal/signalfd.h>
#include <vespera_errno.h>

#include <uapi/vespera/signalfd.h>
#include <uapi/vespera/signal.h>

#include "realm/realm.h"
#include "sys/handle_resolution.h"
#include "vespera/scheduling.h"
#include "vespera/realm/handles.h"

class Realm;

namespace syscalls::internal {
    i64 sys_signalfd4(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const auto ufd = static_cast<i32>(arg0);
        const auto* user_mask = reinterpret_cast<const sigset_t*>(arg1);
        const usize sizemask = static_cast<usize>(arg2);
        const u32 flags = static_cast<u32>(arg3);

        constexpr u32 valid_flags = SFD_CLOEXEC | SFD_NONBLOCK;
        if (flags & ~valid_flags) return -EINVAL;
        if (!user_mask) return -EFAULT;
        if (sizemask != sizeof(sigset_t)) return -EINVAL;

        const u64 mask = *reinterpret_cast<const u64*>(user_mask);

        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        // ufd == -1 -> new Signalfd
        if (ufd < 0) {
            auto* sfd = Signalfd::create(realm, mask, flags & SFD_NONBLOCK);
            if (!sfd) return -ENOMEM;

            auto hdl_result = kernel::realm::add_handle_to_current(
                HANDLE_TYPE_SIGNALFD, sfd, CAP_READ, true, Signalfd::destroy, Signalfd::ref);
            if (hdl_result.is_err()) {
                Signalfd::destroy(sfd);
                return -EMFILE;
            }

            realm->signalfd_list.push(sfd);

            return static_cast<i64>(hdl_result.unwrap());
        }

        // ufd >= 0 -> update mask
        auto rh_result = resolve_handle(static_cast<HandleId>(ufd));
        if (rh_result.is_err()) return -EBADH;

        const auto& rh = rh_result.unwrap();
        if (rh.type() != HANDLE_TYPE_SIGNALFD) return -EINVAL;

        auto* sfd = rh.resource_as<Signalfd>();
        if (!sfd) return -EINVAL;

        sfd->set_mask(mask);

        return ufd;
    }
} // namespace syscalls::internal