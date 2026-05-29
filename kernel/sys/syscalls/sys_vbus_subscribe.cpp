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

#include <uapi/vespera/handles.h>
#include <uapi/vespera/vbus.h>
#include <vespera/ipc/channel.h>
#include <vespera/ipc/vbus_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "../handle_resolution.h"

namespace syscalls::internal {

    i64 sys_vbus_subscribe(u64 arg0, u64, u64, u64, u64, u64) {
        const auto* args = reinterpret_cast<const vbus_subscribe_args_t*>(arg0);

        if (!args) return -EINVAL;

        const auto rh = SYSCALL_TRY(resolve_handle(HANDLE_VBUS, HANDLE_TYPE_CHANNEL));

        auto* ch = rh.resource_as<Channel>();
        if (!ch) return -ENOENT;

        const RealmId caller_id = kernel::scheduling::get_current_realm_id();

        if (caller_id == 0) return -ESRCH;

        return VBusManager::subscribe(caller_id, ch, args->interface, args->member);
    }

    i64 sys_vbus_unsubscribe(u64, u64, u64, u64, u64, u64) {
        const RealmId caller_id = kernel::scheduling::get_current_realm_id();

        if (caller_id == 0) return -ESRCH;

        VBusManager::unsubscribe_realm(caller_id);

        return SUCCESS_CODE;
    }

    i64 sys_vbus_emit(u64 hdr_uptr, u64 payload_uptr, u64 payload_len, u64, u64, u64) {
        if (!hdr_uptr) return -EINVAL;
        if (payload_len > VBUS_MAX_PAYLOAD_SIZE) return -EINVAL;

        vbus_header_t hdr = *reinterpret_cast<vbus_header_t*>(hdr_uptr);

        if (hdr.magic != VBUS_MAGIC) return -EINVAL;

        const auto rh = SYSCALL_TRY(resolve_handle(HANDLE_VBUS, HANDLE_TYPE_CHANNEL));

        auto* ch = rh.resource_as<Channel>();
        if (!ch) return -ENOENT;

        RealmId rid = kernel::scheduling::get_current_realm_id();
        return VBusManager::emit_from_realm(rid, ch, &hdr, reinterpret_cast<void*>(payload_uptr), payload_len);
    }

}  // namespace syscalls::internal