// sys_socketpair.cpp
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

#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <uapi/vespera/socket.h>
#include <vespera/ipc/socket.h>
#include <vespera/ipc/socket_handle.h>
#include <vespera/types.h>

#include "sys/user_copy.h"
#include "vespera/realm/handles.h"

namespace syscalls::internal {
    i64 sys_socketpair(const u64 arg0, const u64 arg1, const u64 arg2, const u64 arg3, u64, u64) {
        const auto domain = static_cast<i32>(arg0);
        const auto type = static_cast<i32>(arg1);
        const auto protocol = static_cast<i32>(arg2);
        const auto user_sv = reinterpret_cast<u64*>(arg3);

        if (domain != AF_UNIX) return -EAFNOSUPPORT;
        if (type != SOCK_STREAM) return -EPROTOTYPE;
        if (protocol != 0) return -EPROTONOSUPPORT;
        if (!user_sv) return -EFAULT;

        constexpr usize default_capacity = 64 * 1024;

        auto pair_res = SocketEndpoint::create_connected_pair(default_capacity);
        if (pair_res.is_err()) return pair_res.to_errno();

        SocketPair pair = pair_res.unwrap();

        SocketHandle* handle_a = SocketHandle::create();
        SocketHandle* handle_b = handle_a ? SocketHandle::create() : nullptr;

        if (!handle_a || !handle_b) {
            if (handle_a) SocketHandle::destroy(handle_a);
            SocketEndpoint::destroy(pair.a);
            SocketEndpoint::destroy(pair.b);
            return -ENOMEM;
        }

        handle_a->state = SocketState::CONNECTED;
        handle_a->endpoint = pair.a;
        handle_b->state = SocketState::CONNECTED;
        handle_b->endpoint = pair.b;

        const Result<HandleId> res_a = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_SOCKET, handle_a, CAP_READ | CAP_WRITE,
            /*transferable=*/true, SocketHandle::destroy, SocketHandle::ref
        );
        if (res_a.is_err()) {
            SocketHandle::destroy(handle_a);
            SocketHandle::destroy(handle_b);
            return res_a.to_errno();
        }

        const Result<HandleId> res_b = kernel::realm::add_handle_to_current(
            HANDLE_TYPE_SOCKET, handle_b, CAP_READ | CAP_WRITE,
            /*transferable=*/true, SocketHandle::destroy, SocketHandle::ref
        );
        if (res_b.is_err()) {
            //kernel::realm::remove_handle_from_current(res_a.unwrap());

            SocketHandle::destroy(handle_b);
            return res_b.to_errno();
        }

        const u64 sv_out[2] = {
            static_cast<u64>(res_a.unwrap()),
            static_cast<u64>(res_b.unwrap())
        };

        if (!copy_to_user(user_sv, sv_out, sizeof(sv_out))) {
            //kernel::realm::remove_handle_from_current(res_a.unwrap());
            //kernel::realm::remove_handle_from_current(res_b.unwrap());
            return -EFAULT;
        }

        return 0;
    }
} // namespace syscalls::internal