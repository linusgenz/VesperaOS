// sys_write.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.08.25.
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

#include <uapi/vespera/handels.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../../filesystem/vfs/vfs_handle.h"
#include "../syscall_interface.h"

namespace syscalls::internal {
    i64 sys_write(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const auto buf = reinterpret_cast<void *>(arg1);
        const usize count = arg2;

        if (count == 0) {
            return 0;
        }

        const Unit *u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;

        Realm *realm = RealmManager::get(u->rid);

        if (!realm || !u->active) return -EUNKNOWN;

        const HandleEntry *he = realm->lookup_handle(hid);
        if (!he || !he->resource) return -EBADH;

        if (!buf) return -EINVAL;

        if (!(he->capabilities & CAP_WRITE)) {
            return -EACCES;
        }
        switch (he->type) {
            case HANDLE_TYPE_TTY: {
                auto *tty_dev = static_cast<TtyDevice *>(he->resource);
                return tty_dev->write(nullptr, buf, count);
            }
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const auto *vh = static_cast<VfsHandle *>(he->resource);
                if (!vh) return -EBADH;
                const isize bytes = VFS::write(vh->node, vh->context->position, count, buf);
                if (bytes > 0) {
                    vh->context->position += bytes;
                }
                return bytes;
            }
            case HANDLE_TYPE_PIPE: {
                auto* ch = static_cast<Channel*>(he->resource);
                isize r;
                while ((r = ch->send(buf, count)) == -EAGAIN) {
                   // kernel::scheduling::yield();
                    asm volatile("pause");
                }
                return r;
            }
            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
