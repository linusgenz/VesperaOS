// sys_poll.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.03.26.
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
#include <sys/handle_resolution.h>
#include <uapi/vespera/fcntl.h>
#include <uapi/vespera/handles.h>
#include <uapi/vespera/poll.h>
#include <vespera/ipc/eventfd.h>
#include <vespera/ipc/socket_handle.h>
#include <vespera/scheduling.h>
#include <vespera/signal/signalfd.h>
#include <vespera/time/timerfd.h>
#include <vespera/time.h>
#include <vespera/types.h>

#include "filesystem/vfs_handle.h"

namespace syscalls::internal {
    i64 sys_poll(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        auto* hdls = reinterpret_cast<pollhdl*>(arg0);
        const usize nhdls = arg1;
        const i32 timeout_ms = static_cast<i32>(arg2);

        if (!hdls || nhdls == 0) return -EINVAL;

        const Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        const u64 deadline = (timeout_ms >= 0) ? kernel::time::get_uptime_ms() + static_cast<u64>(timeout_ms) : U64_MAX;

        while (true) {
            int ready = 0;

            for (usize i = 0; i < nhdls; ++i) {
                hdls[i].revents = 0;
                const auto rh_result = resolve_handle(hdls[i].hdl);

                if (rh_result.is_err()) {
                    hdls[i].revents = POLLHUP;
                    ready++;
                    continue;
                }

                const auto& rh = rh_result.unwrap();

                int mask = 0;
                bool bad_handle = false;

                switch (rh.type()) {
                    case HANDLE_TYPE_CHANNEL:
                    case HANDLE_TYPE_PIPE: {
                        auto* ep = rh.resource_as<ChannelEndpoint>();
                        if (!ep) return -EINVAL;

                        Channel* ch = ep->channel;
                        if (!ch) {
                            bad_handle = true;
                            break;
                        }

                        mask = ch->poll(ep->is_reader, ep->is_writer);
                        break;
                    }

                    case HANDLE_TYPE_FIFO: {
                        const auto* vh = rh.resource_as<VfsHandle>();

                        if (!vh || !vh->node || !vh->node->fifo_channel) {
                            bad_handle = true;
                            break;
                        }

                        const u32 acc = vh->context->open_flags & 0x3;
                        const bool is_reader = acc == O_RDONLY || acc == O_RDWR;
                        const bool is_writer = acc == O_WRONLY || acc == O_RDWR;

                        mask = vh->node->fifo_channel->poll(is_reader, is_writer);
                        break;
                    }

                    case HANDLE_TYPE_SOCKET: {
                        auto* sh = rh.resource_as<SocketHandle>();

                        if (!sh || sh->state != SocketState::CONNECTED || !sh->endpoint) {
                            bad_handle = true;
                            break;
                        }

                        mask = sh->endpoint->poll();
                        break;
                    }

                    case HANDLE_TYPE_EVENTFD: {
                        auto* efd = rh.resource_as<Eventfd>();
                        if (!efd) {
                            bad_handle = true;
                            break;
                        }

                        const u32 acc = hdls[i].events & (POLLIN | POLLOUT);
                        mask = efd->poll(acc & POLLIN, acc & POLLOUT);
                        break;
                    }

                    case HANDLE_TYPE_TIMERFD: {
                        auto* tfd = rh.resource_as<Timerfd>();
                        if (!tfd) {
                            bad_handle = true;
                            break;
                        }

                        mask = tfd->poll(/*is_reader=*/true, /*is_writer=*/false);
                        break;
                    }

                    case HANDLE_TYPE_SIGNALFD: {
                        auto* sfd = rh.resource_as<Signalfd>();
                        if (!sfd) {
                            bad_handle = true;
                            break;
                        }

                        mask = sfd->poll(/*is_reader=*/true, /*is_writer=*/false);
                        break;
                    }

                    default: {
                        const auto* vh = rh.resource_as<VfsHandle>();

                        if (!vh || !vh->node) {
                            bad_handle = true;
                            break;
                        }

                        if (vh->node->ops && vh->node->ops->poll) mask = vh->node->ops->poll(vh->node, vh->context);
                        break;
                    }
                }

                if (bad_handle) {
                    hdls[i].revents = POLLHUP;
                    ready++;
                    continue;
                }

                const int always_reported = mask & (POLLERR | POLLHUP);
                const int requested = mask & hdls[i].events & ~(POLLERR | POLLHUP);
                hdls[i].revents = static_cast<i16>(always_reported | requested);
                if (hdls[i].revents) ready++;
            }

            if (ready > 0) return ready;
            if (timeout_ms == 0) return 0;
            if (kernel::time::get_uptime_ms() >= deadline) return 0;

            kernel::scheduling::yield();
        }
    }
} // namespace syscalls::internal
