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

#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <vespera/ipc/channel.h>
#include <vespera/realm/realm.h>
#include <vespera/realm/realm_manager.h>
#include <vespera_errno.h>

#include "../handle_resolution.h"

namespace syscalls::internal {

    i64 sys_handle_transfer(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const RealmId target_realm_id = arg1;
        const capability_set caps_mask = static_cast<capability_set>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (!rh.entry->transferable) return -EACCES;

        const capability_set granted = rh.entry->capabilities & caps_mask;
        if (granted == 0) return -EACCES;

        const u64 type_tag = rh.type();
        if (type_tag != HANDLE_TYPE_CHANNEL && type_tag != HANDLE_TYPE_PIPE) return -EACCES;

        Realm* dst_realm = RealmManager::get(target_realm_id);
        if (!dst_realm) return -ECHILD;

        auto* ch = rh.resource_as<Channel>();
        if (!ch) return -EINVAL;

        Channel::ref(ch);

        const Result<HandleId> result = dst_realm->handle_table->add(
            type_tag, ch, CAP_RW, /*transferable=*/true, Channel::destroy, /*acquire=*/nullptr
        );

        if (result.is_err()) {
            Channel::destroy(ch);
            return result.to_errno();
        }

        return static_cast<i64>(result.unwrap());
    }
}  // namespace syscalls::internal
