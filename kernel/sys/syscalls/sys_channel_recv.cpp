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

#include <kernel/units/unit.h>
#include "../handle_resolution.h"
#include "uapi/vespera/handles.h"

namespace syscalls::internal {
    i64 sys_channel_recv(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto hid = arg0;
        const auto buf = reinterpret_cast<void*>(arg1);
        const auto len = arg2;

        const auto rh = SYSCALL_TRY(resolve_handle(hid, HANDLE_TYPE_CHANNEL, CAP_READ));

        auto* ch = rh.resource_as<Channel>();
        if (!ch) return -EINVAL;

        const int res = ch->recv(buf, len);
        if (res < 0) return -EAGAIN;

        return res;
    }
}  // namespace syscalls::internal
