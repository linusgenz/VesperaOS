// sys_bind.cpp
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
#include "uapi/vespera/socket.h"
#include "uapi/vespera/un.h"

namespace syscalls::internal {
    i64 sys_bind(const u64 arg0, const u64 arg1, const u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto user_addr = reinterpret_cast<const sockaddr*>(arg1);
        const auto addrlen = static_cast<socklen_t>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid, HANDLE_TYPE_SOCKET, CAP_WRITE));
        auto* handle = rh.resource_as<SocketHandle>();
        if (!handle) return -EBADH;
        if (handle->state != SocketState::UNBOUND) return -EINVAL;

        char raw_path[sizeof(sockaddr_un::sun_path)];
        SYSCALL_TRY_VOID(extract_unix_path(user_addr, addrlen, raw_path, sizeof(raw_path)));

        char norm[256];
        SYSCALL_TRY_VOID(VFS::resolve_path(raw_path, norm, sizeof(norm)));

        constexpr mode_t sock_mode = 0xC000u | 0666u; // S_IFSOCK | perms; TODO honor umask
        auto create_res = VFS::create(norm, sock_mode);

        if (create_res.is_err()) {
            if (create_res.error() != Error::Exist) return create_res.to_errno();

            // Path already exists: for sockets we return EADDRINUSE  instead
            auto existing_res = VFS::open(norm);
            if (existing_res.is_err()) return existing_res.to_errno();

            VfsNode* existing = existing_res.unwrap();
            VFS::close(existing);
            return -EADDRINUSE;
        }

        auto node_res = VFS::open(norm);
        if (node_res.is_err()) return node_res.to_errno();

        VfsNode* node = node_res.unwrap();

        if (node->type != VfsNodeType::Socket) {
            VFS::close(node);
            return -EINVAL;
        }

        if (node->socket_listener) {
            VFS::close(node);
            return -EADDRINUSE;
        }

        auto listener_res = SocketListener::bind(norm);
        if (listener_res.is_err()) {
            VFS::close(node);
            return listener_res.to_errno();
        }

        node->socket_listener = listener_res.unwrap();

        handle->state = SocketState::BOUND;
        handle->listener = node->socket_listener;
        handle->bound_node = node;

        return 0;
    }
}