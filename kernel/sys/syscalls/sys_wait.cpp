// sys_wait.cpp
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

#include <kernel/realm/realm_manager.h>
#include <kernel/scheduling.h>

#include "../../units/unit.h"

namespace syscalls::internal {
    int64_t sys_wait(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
        realm_id_t child_rid = arg0;
        int64_t status_user_ptr = static_cast<int64_t>(arg1);

        Unit* current = kernel::scheduling::get_current_unit();
        if (!current) return -EINVAL;

        Realm* target = RealmManager::get(child_rid);
        if (!target) {
            return -ECHILD;
        }

        if (target->unit_count == 0) {
            if (status_user_ptr != 0) {
                constexpr int status_val = 0;
                (*reinterpret_cast<int*>(status_user_ptr)) = status_val;
            }
            return 0;
        }

        target->wait_queue.add_wait(current);

        kernel::scheduling::yield();

        if (status_user_ptr != 0) {
            int status_val = 0;
            (*reinterpret_cast<int*>(status_user_ptr)) = status_val;
        }

        return 0;
    }
}  // namespace syscalls::internal
