// sys_read.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.08.25.
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

#include <filesystem/vfs.h>
#include <tty/tty_device.h>
#include <uapi/vespera/handles.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>

#include "filesystem/vfs_handle.h"
#include "sys/handle_resolution.h"
#include "vespera/ipc/eventfd.h"
#include "vespera/ipc/socket_handle.h"
#include "vespera/time/timerfd.h"

namespace syscalls::internal {
    i64 sys_read(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto buf = reinterpret_cast<void*>(arg1);
        const usize count = arg2;

        if (!buf || count == 0) return -EINVAL;

        const auto rh = SYSCALL_TRY(resolve_handle(hid, /*type_mask=*/0, CAP_READ));

        switch (rh.type()) {
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FIFO:
            case HANDLE_TYPE_FILE: {
                const auto* vh = rh.resource_as<VfsHandle>();
                if (!vh) return -EBADH;

                const i64 off = VFS::is_seekable(vh->node) ? vh->context->position : -1;
                const usize bytes = SYSCALL_TRY(VFS::read(vh->node, static_cast<usize>(off < 0 ? 0 : off), count, buf, vh->context, vh->context->open_flags));
                if (bytes > 0 && off >= 0) vh->context->position += bytes;
                return static_cast<isize>(bytes);
            }
            case HANDLE_TYPE_PIPE: {
                const auto* ep = rh.resource_as<ChannelEndpoint>();
                if (!ep) return -EINVAL;

                if (ep->is_writer) {
                    return -EBADH;
                }

                auto* ch = ep->channel;
                isize r = 0;
                while ((r = ch->recv(buf, count)) == -EAGAIN) {
                    kernel::scheduling::yield();
                }
                return r;
            }
            case HANDLE_TYPE_SOCKET: {
                const auto* sh = rh.resource_as<SocketHandle>();
                if (!sh) return -EBADH;
                if (sh->state != SocketState::CONNECTED || !sh->endpoint) return -ENOTCONN;

                const bool nonblock = false; // TODO: handle flags integrated with fcntl
                const isize r = sh->endpoint->recv(buf, count, !nonblock);
                return r;
            }
            case HANDLE_TYPE_TIMERFD: {
                auto* tfd = rh.resource_as<Timerfd>();
                if (!tfd) return -EBADH;
                return tfd->read(buf, count);
            }
            case HANDLE_TYPE_SIGNALFD: {
                auto* sfd = rh.resource_as<Signalfd>();
                if (!sfd) return -EBADH;
                return sfd->read(buf, count);
            }
            case HANDLE_TYPE_EVENTFD: {
                auto* efd = rh.resource_as<Eventfd>();
                if (!efd) return -EBADH;
                return efd->read(buf, count);
            }
            default:
                return -EBADH;
        }
    }
} // namespace syscalls::internal
