// sys_connect.cpp
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

#include "filesystem/vfs.h"
#include "filesystem/vfs_node.h"
#include "sys/handle_resolution.h"
#include "vespera/ipc/socket_handle.h"
#include "vespera/ipc/socket_helper.h"
#include "uapi/vespera/un.h"

namespace syscalls::internal {
    i64 sys_connect(const u64 arg0, const u64 arg1, const u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto user_addr = reinterpret_cast<const sockaddr*>(arg1);
        const auto addrlen = static_cast<socklen_t>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid, HANDLE_TYPE_SOCKET, CAP_WRITE));
        auto* handle = rh.resource_as<SocketHandle>();
        if (!handle) return -EBADH;
        if (handle->state != SocketState::UNBOUND) return -EISCONN; // already bound or connected

        char raw_path[sizeof(sockaddr_un::sun_path)];
        SYSCALL_TRY_VOID(extract_unix_path(user_addr, addrlen, raw_path, sizeof(raw_path)));

        char norm[256];
        SYSCALL_TRY_VOID(VFS::resolve_path(raw_path, norm, sizeof(norm)));

        auto node_res = VFS::open(norm);
        if (node_res.is_err()) return -ECONNREFUSED; // no such socket

        VfsNode* node = node_res.unwrap();

        if (node->type != VfsNodeType::Socket || !node->socket_listener) {
            VFS::close(node);
            return -ECONNREFUSED;
        }

        SocketListener* listener = node->socket_listener;
        VFS::close(node);

        constexpr usize default_capacity = 64 * 1024; // TODO: SO_SNDBUF/RCVBUF later
        auto connect_res = listener->connect(default_capacity, /*blocking=*/true);
        if (connect_res.is_err()) return connect_res.to_errno();

        handle->state = SocketState::CONNECTED;
        handle->endpoint = connect_res.unwrap();

        return 0;
    }
}
