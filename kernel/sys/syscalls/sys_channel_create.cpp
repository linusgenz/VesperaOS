// sys_channel_create.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include <kernel/ipc/channel.h>
#include <kernel/realm/realm_manager.h>
#include <kernel/scheduling.h>

#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_channel_create(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        size_t capacity = arg0;
        if (capacity == 0) capacity = 4096;  // default size

        const Unit* current_unit = kernel::scheduling::get_current_unit();
        if (!current_unit) return -EINVAL;
        Realm* realm = RealmManager::get(current_unit->rid);
        if (!realm) return -EINVAL;

        Channel* ch = Channel::create(capacity);
        if (!ch) return -ENOMEM;

        // set required caps for channels: read+write for owner
        CapabilitySet caps = CAP_READ | CAP_WRITE;

        HandleID hid;

        if (const ErrorCode err = realm->add_handle(HANDLE_TYPE_CHANNEL, ch, caps, true, Channel::destroy, &hid);
            err != MOD_SUCCESS) {
            Channel::destroy(ch);
            return -err;
        }

        return hid;
    }
}  // namespace syscalls::internal
