// sys_channel_send.cpp
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

#include <vespera_errno.h>
#include <kernel/ipc/channel.h>
#include <kernel/realm/realm_manager.h>
#include <kernel/scheduling.h>

#include "../../types/types.h"
#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_channel_send(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        const auto hid = arg0;
        const auto data = reinterpret_cast<const void*>(arg1);
        const auto len = arg2;

        Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;
        Realm* realm = RealmManager::get(u->rid);
        if (!realm) return -EINVAL;

        handle_entry_t* he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_WRITE)) return -EACCES;
        if ((he->type & HANDLE_TYPE_MASK) != HANDLE_TYPE_CHANNEL) return -EINVAL;

        auto* ch = static_cast<Channel*>(he->resource);
        if (!ch) return -EINVAL;

        int res = ch->send(data, len);
        if (res < 0) return -EAGAIN;
        return res;  // bytes written
    }
}  // namespace syscalls::internal
