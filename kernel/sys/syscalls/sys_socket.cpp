// sys_socket.cpp
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

#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <uapi/vespera/socket.h>
#include <vespera/ipc/socket_handle.h>
#include <vespera/types.h>

#include "vespera/realm/handles.h"

namespace syscalls::internal {
    i64 sys_socket(const u64 arg0, const u64 arg1, const u64 arg2, u64, u64, u64) {
        const auto domain = static_cast<i32>(arg0);
        const auto type = static_cast<i32>(arg1);
        const auto protocol = static_cast<i32>(arg2);

        if (domain != AF_UNIX) return -EAFNOSUPPORT;
        if (type != SOCK_STREAM) return -EPROTOTYPE;
        if (protocol != 0) return -EPROTONOSUPPORT;

        SocketHandle* handle = SocketHandle::create();
        if (!handle) return -ENOMEM;

        const Result<HandleId> result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_SOCKET,
            handle,
            CAP_READ | CAP_WRITE,
            /*transferable=*/true,
            SocketHandle::destroy,
            SocketHandle::ref
        );

        if (result.is_err()) {
            SocketHandle::destroy(handle);
            return result.to_errno();
        }

        return static_cast<i64>(result.unwrap());
    }
} // namespace syscalls::internal
