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

#include <uapi/vespera/handles.h>
#include <vespera/filesystem/vfs.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>

#include "../filesystem/vfs/vfs_handle.h"

namespace syscalls::internal {
    i64 sys_read(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto buf = reinterpret_cast<void *>(arg1);
        const usize count = arg2;

        if (!buf || count == 0) return -EINVAL;

        const Unit *u = kernel::scheduling::get_current_unit();
        if (!u || !u->active) return -EINVAL;

        Realm *realm = u->parent;
        if (!realm) return -EUNKNOWN;

        const HandleEntry *he = realm->handle_table.lookup(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_READ)) {
            return -EACCES;
        }

        switch (he->type & HANDLE_TYPE_MASK) {
            case HANDLE_TYPE_TTY: {
                auto *tty_dev = static_cast<TtyDevice *>(he->resource);
                if (!tty_dev) return 0;
                return tty_dev->read(nullptr, buf, count, 0);
            }
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const auto* vh = static_cast<VfsHandle*>(he->resource);
                if (!vh) return -EBADH;

                const usize bytes = SYSCALL_TRY(VFS::read(vh->node, vh->context->position, count, buf));
                if (bytes > 0) vh->context->position += bytes;
                return static_cast<isize>(bytes);
            }
            case HANDLE_TYPE_PIPE: {
                auto* ch = static_cast<Channel*>(he->resource);
                isize r;
                while ((r = ch->recv(buf, count)) == -EAGAIN) {
                    kernel::scheduling::yield();
                }
                return r;
            }
            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
