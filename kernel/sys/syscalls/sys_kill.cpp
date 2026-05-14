// sys_kill.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.03.26.
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

#include <realm/realm.h>
#include <scheduling/unit_termination.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/realm_ops.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_kill(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        if (!is_valid_signal(static_cast<i32>(arg1))) return -EINVAL;

        if (!kernel::scheduling::get_current_unit()) return -EINVAL;

        const auto sig = static_cast<Signal>(static_cast<i32>(arg1));
        const i64 target = static_cast<i64>(arg0);

        if (target < 0) {
            RealmManager::signal_pgid(static_cast<RealmId>(-target), sig);
            return SUCCESS_CODE;
        }

        const auto target_rid = static_cast<RealmId>(target);
        if (target_rid == 0) return -EINVAL;

        if (sig == Signal::SIGKILL) return kernel::scheduling::kill_realm_by_id(target_rid, sig);

        SYSCALL_TRY_VOID(kernel::realm::send_signal(target_rid, sig));

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal