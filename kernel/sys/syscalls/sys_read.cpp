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

#include <scheduling.h>

#include "../../../filesystem/vfs/vfs.h"
#include "../tty/tty.h"
#include "../../../include/log.h"
#include "../../include/errno.h"
#include "../../realm/realm_manager.h"
#include "../types/types.h"
#include "../graphics/console_backend.h"
#include "../filesystem/vfs/vfs_handle.h"

namespace syscalls::internal {
    static Unit* reader_owner = nullptr;
    int64_t sys_read(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        if (reader_owner != nullptr && reader_owner != kernel::scheduling::get_current_unit()) {
            return -EAGAIN;
        }
        reader_owner = kernel::scheduling::get_current_unit();

        HandleID hid = arg0;
        void *buf = reinterpret_cast<void *>(arg1);
        size_t count = static_cast<size_t>(arg2);

        if (!buf || count == 0) return -EINVAL;

        Unit *u = kernel::scheduling::get_current_unit();
        if (!u || !u->active) return -EINVAL;

        Realm *realm = RealmManager::get(u->rid);
        if (!realm) return -EUNKNOWN;

        handle_entry_t *he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_READ)) {
            return -EACCES;
        }

        switch (he->type & HANDLE_TYPE_MASK) {
            case HANDLE_TYPE_CONSOLE: {
                ConsoleDevice *cons = static_cast<ConsoleDevice *>(he->resource);
                return cons->read(reinterpret_cast<char *>(buf), count);
            }
            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                VfsHandle *vh = static_cast<VfsHandle *>(he->resource);
                if (!vh || !vh->node || !vh->node->ops || !vh->node->ops->read) return -EBADH;
                size_t bytes = vh->node->ops->read(vh->node, vh->context->position, count, buf);
                vh->context->position += bytes;
                return bytes;
            }
            default:
                return -EBADH;
        }
    }
}
