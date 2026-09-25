// sys_accept.cpp
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
#include "uapi/vespera/capabilities.h"
#include "uapi/vespera/socket.h"
#include "uapi/vespera/handles.h"
#include "vespera/ipc/socket_handle.h"
#include "vespera/realm/handles.h"

namespace syscalls::internal {
    i64 sys_accept4(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const HandleId listen_hid = arg0;
        const auto user_addr = reinterpret_cast<sockaddr*>(arg1);
        const auto user_addrlen = reinterpret_cast<socklen_t*>(arg2);
        const auto flags = static_cast<int>(arg3);

        if (flags & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) return -EINVAL;
        const bool blocking = !(flags & SOCK_NONBLOCK);

        const auto rh = SYSCALL_TRY(resolve_handle(listen_hid, HANDLE_TYPE_SOCKET, CAP_READ));

        auto* listen_handle = rh.resource_as<SocketHandle>();
        if (!listen_handle) return -EBADH;
        if (listen_handle->state != SocketState::LISTENING || !listen_handle->listener) return -EINVAL;

        // addr/addrlen are an out-pair: both null, or both non-null. Mixed
        // is a caller bug, same as Linux's accept(2).
        if ((user_addr != nullptr) != (user_addrlen != nullptr)) return -EINVAL;

        socklen_t caller_len = 0;
        if (user_addrlen) caller_len = *user_addrlen;

        auto accept_res = listen_handle->listener->accept(blocking);
        if (accept_res.is_err()) return accept_res.to_errno();

        SocketHandle* new_handle = SocketHandle::create();
        if (!new_handle) {
            SocketEndpoint::destroy(accept_res.unwrap());
            return -ENOMEM;
        }
        new_handle->state = SocketState::CONNECTED;
        new_handle->endpoint = accept_res.unwrap();

        const Result<HandleId> result = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_SOCKET, new_handle, CAP_READ | CAP_WRITE, true,
            SocketHandle::destroy, SocketHandle::ref);

        if (result.is_err()) {
            SocketHandle::destroy(new_handle);
            return result.to_errno();
        }

        if (user_addr && user_addrlen) {
            constexpr socklen_t real_len = sizeof(sa_family_t);
            if (caller_len >= sizeof(sa_family_t)) {
                user_addr->sa_family = AF_UNIX;
            }
            *user_addrlen = real_len;
        }

        return static_cast<i64>(result.unwrap());
    }
}