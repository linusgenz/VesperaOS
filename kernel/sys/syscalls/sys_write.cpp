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

#include <scheduling.h>

#include "../../include/errno.h"
#include "../syscall_interface.h"
#include "../../../include/log.h"
#include "../../realm/realm_manager.h"

namespace syscalls::internal {
    int64_t sys_write(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        const HandleID hid = arg0;
        Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;

        Realm* realm = RealmManager::get(u->rid);

        if (!realm || !u->active) return -EUNKNOWN;

        handle_entry_t* he = realm->lookup_handle(hid);
        if (!he || !he->resource) return -EBADH;

        const char* user_buf = reinterpret_cast<const char*>(arg1);
        if (!user_buf || arg2 == 0) return -EINVAL;

        if (!(he->capabilities & CAP_WRITE)) {
            return -EACCES;
        }

        switch (he->type) {
            case HANDLE_TYPE_TTY: {
                auto* tty_dev = static_cast<TTYDevice*>(he->resource);
                return tty_dev->write(nullptr, user_buf, arg2);
            }
            case HANDLE_TYPE_FILE: {
             /*   FileNode* node = static_cast<FileNode*>(he->resource);
                FileDescriptor* desc = kernel::get_fd(hid); // optional, wenn du FileDescriptors nutzt
                if (!desc || !desc->node->ops || !desc->node->ops->write) return -EBADF;

                size_t bytes = desc->node->ops->write(desc->node, desc->offset, arg2, user_buf);
                desc->offset += bytes;
                return bytes;*/
                return 0;
            }
            default:
                return -EBADH;
        }
    }
}