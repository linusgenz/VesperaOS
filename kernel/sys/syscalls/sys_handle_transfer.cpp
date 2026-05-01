// sys_handle_transfer.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 21.04.26.
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
#include <vespera/ipc/channel.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "../../units/unit.h"

namespace syscalls::internal {

    i64 sys_handle_transfer(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const RealmId target_realm_id = arg1;
        const capability_set caps_mask = static_cast<capability_set>(arg2);

        const Unit* caller = kernel::scheduling::get_current_unit();
        if (!caller) return -EINVAL;

        Realm* src_realm = caller->parent;
        if (!src_realm) return -EINVAL;

        HandleEntry* src_he = src_realm->lookup_handle(hid);
        if (!src_he) return -EBADH;

        if (!src_he->transferable) return -EACCES;

        const capability_set granted = src_he->capabilities & caps_mask;
        if (granted == 0) return -EACCES;

        Realm* dst_realm = RealmManager::get(target_realm_id);
        if (!dst_realm) return -ECHILD;

        const u64 type_tag = src_he->type & HANDLE_TYPE_MASK;

        if (type_tag != HANDLE_TYPE_CHANNEL && type_tag != HANDLE_TYPE_PIPE) {
            // Only ref-counted kernel objects make sense to transfer.
            return -EACCES;
        }

        auto* ch = static_cast<Channel*>(src_he->resource);
        if (!ch) return -EINVAL;

        Channel::ref(ch);

        HandleId new_hid = 0;

        const i64 err = dst_realm->add_handle(
            type_tag,
            ch,
            CAP_RW,
            /*transferable=*/true,
            Channel::destroy,
            /*acquire=*/nullptr,
            &new_hid
        );

        if (err != SUCCESS_CODE) {
            Channel::destroy(nullptr);
            return err;
        }

        return static_cast<i64>(new_hid);
    }

}  // namespace syscalls::internal
