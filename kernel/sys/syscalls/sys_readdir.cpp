// sys_readdir.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.09.25.
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
#include "../../../include/vespera/types.h"
#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_readdir(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
        HandleId hid = arg0;
        auto *ent = reinterpret_cast<dirent_t *>(arg1);

        if (!ent) return -EINVAL;

        Unit *u = kernel::scheduling::get_current_unit();
        Realm *realm = RealmManager::get(u->rid);
        HandleEntry *he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_READ)) return -EACCES;
        if ((he->type & HANDLE_TYPE_MASK) != HANDLE_TYPE_DIRECTORY) return -EINVAL;

        const auto *vh = static_cast<VfsHandle *>(he->resource);
        if (!vh->context || !vh->context->type_specific_data) return -EINVAL;

        void *dir_handle = vh->context->type_specific_data;
        if (!vh->node->ops || !vh->node->ops->readdir) return -EINVAL;

        return vh->node->ops->readdir(dir_handle, ent);
    }
}  // namespace syscalls::internal
