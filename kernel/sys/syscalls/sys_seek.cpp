// sys_seek.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 04.10.25.
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

#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../../filesystem/vfs/vfs_handle.h"
#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_seek(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        handle_id_t hid = arg0;
        int64_t offset = static_cast<int64_t>(arg1);
        int whence = static_cast<int>(arg2);

        Unit *u = kernel::scheduling::get_current_unit();
        if (!u || !u->active) return -EINVAL;

        Realm *realm = RealmManager::get(u->rid);
        if (!realm) return -EUNKNOWN;

        handle_entry_t *he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        switch (he->type & HANDLE_TYPE_MASK) {
            case HANDLE_TYPE_DIRECTORY:
            case HANDLE_TYPE_TTY:
                return -ESPIPE;  // Illegal seek

            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const VfsHandle *vh = static_cast<VfsHandle *>(he->resource);
                if (!vh || !vh->node) return -EBADH;

                int64_t new_pos = 0;

                switch (whence) {
                    case SEEK_SET:
                        new_pos = offset;
                        break;

                    case SEEK_CUR:
                        new_pos = vh->context->position + offset;
                        break;

                    case SEEK_END: {
                        new_pos = vh->node->size + offset;
                        break;
                    }

                    default:
                        return -EINVAL;
                }

                // negative seek is invalid
                if (new_pos < 0) return -EINVAL;

                vh->context->position = new_pos;
                return new_pos;
            }

            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
