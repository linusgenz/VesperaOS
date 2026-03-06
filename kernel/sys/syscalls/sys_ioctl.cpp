// sys_ioctl.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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

#include <kernel/realm/realm_manager.h>
#include <kernel/scheduling.h>

#include "../../../filesystem/vfs/vfs_handle.h"
#include "../../types/types.h"
#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_ioctl(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        handle_id_t hid = arg0;
        uint64_t req = arg1;
        auto arg = reinterpret_cast<void*>(arg2);

        const Unit* u = kernel::scheduling::get_current_unit();
        if (!u || !u->active) return -EINVAL;

        Realm* realm = RealmManager::get(u->rid);
        if (!realm) return -EUNKNOWN;

        handle_entry_t* he = realm->lookup_handle(hid);
        if (!he) return -EBADH;

        if (!(he->capabilities & CAP_DEVICE_ACCESS)) {
            return -EACCES;
        }

        switch (he->type & HANDLE_TYPE_MASK) {
            case HANDLE_TYPE_DEVICE: {
                const auto* vh = static_cast<VfsHandle*>(he->resource);
                if (!vh || !vh->node || !vh->node->ops || !vh->node->ops->ioctl) return -ENOTTY;
                return vh->node->ops->ioctl(vh->node, req, arg);
            }
            case HANDLE_TYPE_FILE:
                return -ENOTTY;
            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
