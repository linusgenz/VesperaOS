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
#include <tty/tty_device.h>
#include <uapi/vespera/handles.h>
#include <uapi/vespera/poll.h>
#include <vespera/realm/realm.h>
#include <vespera/scheduling.h>
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

                const HandleEntry* he = realm->handle_table->lookup(hdls[i].hdl);
                if (!he) {
                    hdls[i].revents = POLLHUP;
                    ready++;
                    continue;
                }

                int mask = 0;

                if (he->type == HANDLE_TYPE_TTY) {
                    auto* tty_dev = static_cast<TtyDevice*>(he->resource);
                    if (!tty_dev) {
                        hdls[i].revents = POLLHUP;
                        ready++;
                        continue;
                    }
                    mask = tty_dev->poll(nullptr);

                } else if (he->type == HANDLE_TYPE_CHANNEL) {
                    auto* ch = static_cast<Channel*>(he->resource);
                    if (!ch) {
                        hdls[i].revents = POLLHUP;
                        ready++;
                        continue;
                    }

                    mask = ch->poll();
                } else {
                    const auto* vh = static_cast<VfsHandle*>(he->resource);
                    if (!vh || !vh->node) {
                        hdls[i].revents = POLLHUP;
                        ready++;
                        continue;
                    }
                    if (vh->node->ops && vh->node->ops->poll) mask = vh->node->ops->poll(vh->node);
                }

                hdls[i].revents = static_cast<i16>(mask & hdls[i].events);
                if (hdls[i].revents) ready++;
            }

            if (ready > 0) return ready;
            if (timeout_ms == 0) return 0;
            if (kernel::time::get_uptime_ms() >= deadline) return 0;

            kernel::scheduling::yield();
        }
    }
}  // namespace syscalls::internal
