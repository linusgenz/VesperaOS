// sys_vbus_subscribe.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 03.04.26.
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

#include <vespera/ipc/vbus_manager.h>
#include <uapi/vespera/handles.h>
#include <uapi/vespera/vbus.h>
#include <vespera/ipc/channel.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

namespace syscalls::internal {

    i64 sys_vbus_subscribe(u64 arg0, u64, u64, u64, u64, u64) {
        const auto* args = reinterpret_cast<const vbus_subscribe_args_t*>(arg0);
        if (!args) return -EINVAL;

        const Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;
        Realm* realm = RealmManager::get(u->rid);
        if (!realm) return -EINVAL;

        // Retrieve the realm's vbus channel (HANDLE_VBUS, slot 3)
        const HandleEntry* he = realm->lookup_handle(HANDLE_VBUS);
        if (!he) return -EBADH;
        if ((he->type & HANDLE_TYPE_MASK) != HANDLE_TYPE_CHANNEL) return -EINVAL;

        auto* ch = static_cast<Channel*>(he->resource);
        if (!ch) return -EINVAL;

        return VBusManager::subscribe(u->rid, ch, args->interface, args->member);
    }

    i64 sys_vbus_unsubscribe(u64, u64, u64, u64, u64, u64) {
        const Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;
        VBusManager::unsubscribe_realm(u->rid);
        return SUCCESS_CODE;
    }

}  // namespace syscalls::internal