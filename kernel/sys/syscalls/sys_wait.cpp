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

#include <vespera/realm/exit_code_table.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

namespace syscalls::internal {
    i64 sys_wait(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const RealmId child_rid = arg0;
        const i64 status_user_ptr = static_cast<i64>(arg1);

        Unit* current = kernel::scheduling::get_current_unit();
        if (!current) return -EINVAL;

        Realm* target = RealmManager::get(child_rid);
        if (!target) {
            return -ECHILD;
        }

        {
            SpinlockGuard g(target->lock);
            if (target->unit_count == 0 || target->exited) {
                int exit_code = 0;
                ExitCodeTable::consume(child_rid, &exit_code);
                (*reinterpret_cast<int*>(status_user_ptr)) = exit_code;
                return 0;
            }
        }

        target->wait_queue.add_wait(current);
        kernel::scheduling::yield();

        int exit_code = 0;
        ExitCodeTable::consume(child_rid, &exit_code);
        (*reinterpret_cast<int*>(status_user_ptr)) = exit_code;

        return 0;
    }
}  // namespace syscalls::internal
