// sys_listen.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.09.26.
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

#include <vespera/types.h>

#include "sys/handle_resolution.h"
#include "vespera/ipc/socket_handle.h"

namespace syscalls::internal {
    i64 sys_listen(const u64 arg0, const u64 arg1, u64, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto backlog = static_cast<i32>(arg1);

        const auto rh = SYSCALL_TRY(resolve_handle(hid, HANDLE_TYPE_SOCKET, CAP_WRITE));
        auto* handle = rh.resource_as<SocketHandle>();
        if (!handle) return -EBADH;
        if (handle->state != SocketState::BOUND || !handle->listener) return -EINVAL;

        handle->listener->listen(backlog);
        handle->state = SocketState::LISTENING;

        return 0;
    }
}
