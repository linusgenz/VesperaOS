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

#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../units/unit.h"

namespace syscalls::internal {
    i64 sys_wait(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const RealmId child_rid = arg0;
        const i64 status_user_ptr = static_cast<i64>(arg1);

        Unit* current = kernel::scheduling::get_current_unit();
        if (!current) return -EINVAL;

        const Realm* parent_realm = RealmManager::get(current->rid);

        Realm* target = RealmManager::get(child_rid);
        if (!target) {
            return -ECHILD;
        }

        auto restore_tty_focus = [&]() {
            if (!parent_realm) return;
            TtyDevice* tty_dev = parent_realm->get_tty_device();
            if (tty_dev && tty_dev->tty->fg_realm_id == child_rid) {
                tty_dev->tty->fg_realm_id = parent_realm->id;
            }
        };

        if (target->unit_count == 0) {
            restore_tty_focus();
            if (status_user_ptr != 0) {
                constexpr int status_val = 0;
                (*reinterpret_cast<int*>(status_user_ptr)) = status_val;
            }
            return 0;
        }

        target->wait_queue.add_wait(current);
        kernel::scheduling::yield();

        restore_tty_focus();

        if (status_user_ptr != 0) {
            constexpr int status_val = 0;
            (*reinterpret_cast<int*>(status_user_ptr)) = status_val;
        }

        return 0;
    }
}  // namespace syscalls::internal
