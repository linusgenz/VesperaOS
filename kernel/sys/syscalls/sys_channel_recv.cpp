// sys_channel_recv.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include <vespera/ipc/channel.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../units/unit.h"
#include "uapi/vespera/handels.h"

namespace syscalls::internal {
    i64 sys_channel_recv(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto hid = arg0;
        auto buf = reinterpret_cast<void *>(arg1);
        const auto len = arg2;

        const Unit *u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;
        Realm *realm = RealmManager::get(u->rid);
        if (!realm) return -EINVAL;

        HandleEntry *he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_READ)) return -EACCES;
        if ((he->type & HANDLE_TYPE_MASK) != HANDLE_TYPE_CHANNEL) return -EINVAL;

        auto *ch = static_cast<Channel *>(he->resource);
        if (!ch) return -EINVAL;

        int res = ch->recv(buf, len);
        if (res < 0) return -EAGAIN;
        return res;
    }
}  // namespace syscalls::internal
