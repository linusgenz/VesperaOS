// sys_close.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.08.25.
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

#include <kernel/scheduling.h>

#include "../../../include/errno.h"
#include <kernel/realm/realm_manager.h>
#include "../../types/types.h"

namespace syscalls::internal {
    int64_t sys_close(uint64_t arg0, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        HandleID hid = arg0;

        Unit *current_unit = kernel::scheduling::get_current_unit();
        if (!current_unit) return -EINVAL;

        Realm *realm = RealmManager::get(current_unit->rid);
        if (!realm) return -EINVAL;

        handle_entry_t *he = realm->lookup_handle(hid);
        if (!he) return -EBADH;  // invalid handle

        realm->release_handle(hid);

        return SUCCESS_CODE;
    }
}
