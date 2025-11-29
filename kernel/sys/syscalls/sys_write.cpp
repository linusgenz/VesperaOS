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

#include <kernel/scheduling.h>

#include "../../../include/errno.h"
#include "../syscall_interface.h"
#include "../../../filesystem/vfs/vfs_handle.h"
#include "../../../include/log.h"
#include <kernel/realm/realm_manager.h>

namespace syscalls::internal {
    int64_t sys_write(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        const HandleID hid = arg0;
        auto buf = (void *) arg1;
        size_t count = arg2;
        Unit *u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;

        Realm *realm = RealmManager::get(u->rid);

        if (!realm || !u->active) return -EUNKNOWN;

        handle_entry_t *he = realm->lookup_handle(hid);
        if (!he || !he->resource) return -EBADH;

        const auto user_buf = reinterpret_cast<const char *>(arg1);
        if (!user_buf || arg2 == 0) return -EINVAL;

        if (!(he->capabilities & CAP_WRITE)) {
            return -EACCES;
        }
        switch (he->type) {
            case HANDLE_TYPE_TTY: {
                auto *tty_dev = static_cast<TTYDevice *>(he->resource);
                return tty_dev->write(nullptr, user_buf, arg2);
            }
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const auto *vh = static_cast<VfsHandle *>(he->resource);
                if (!vh || !vh->node || !vh->node->ops || !vh->node->ops->read) return -EBADH;
                const ssize_t bytes = vh->node->ops->write(vh->node, vh->context->position, count, buf);
                vh->context->position += bytes;
                return bytes;
            }
            default:
                return -EBADH;
        }
    }
}
