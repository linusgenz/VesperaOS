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
#include <vespera/realm/handles.h>
#include <vespera/realm/realm_manager.h>
#include <vespera_errno.h>

#include "../handle_resolution.h"

namespace syscalls::internal {
    i64 sys_handle_transfer(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const RealmId target_realm_id = arg1;
        const capability_set caps_mask = arg2;

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        if (!rh.transferable()) return -EACCES;

        const capability_set granted = rh.capabilities() & caps_mask;
        if (granted == 0) return -EACCES;

        const u64 type_tag = rh.type();
        if (type_tag != HANDLE_TYPE_CHANNEL && type_tag != HANDLE_TYPE_PIPE) return -EACCES;

        Realm* dst = RealmManager::get(target_realm_id);
        if (!dst) return -ECHILD;

        auto* ep = rh.resource_as<ChannelEndpoint>(); // we have to adjust, either we make one endpoint or we change semantics here
        if (!ep) return -EINVAL;

        ChannelEndpoint::ref(ep);

        const Result<HandleId> result = kernel::realm::add_handle(
            dst,
            type_tag,
            ep,
            granted,
            /*transferable=*/true,
            ChannelEndpoint::destroy,
            ChannelEndpoint::ref
        );

        if (result.is_err()) {
            ChannelEndpoint::destroy(ep);
            return result.to_errno();
        }

        return static_cast<i64>(result.unwrap());
    }
} // namespace syscalls::internal
