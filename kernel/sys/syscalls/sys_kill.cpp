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

#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>
#include <vespera_errno.h>

#include "../../scheduling/unit_termination.h"

namespace syscalls::internal {
    i64 sys_kill(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const RealmId target_rid = arg0;
        const i32 signum = static_cast<i32>(arg1);

        if (!is_valid_signal(static_cast<i32>(arg1))) return -EINVAL;

        const auto sig = static_cast<Signal>(signum);

        if (const Unit* caller = kernel::scheduling::get_current_unit(); !caller) return -EINVAL;

        const Realm* target = RealmManager::get(target_rid);
        if (!target) return -ESRCH;

        // TODO change signaling so realm gets signal and delegates it do a unit
        if (sig == Signal::SIGKILL) {
            return kernel::scheduling::kill_realm_by_id(target_rid, sig);
        }

        Unit* u = target->unit_list;
        if (!u) return -ESRCH;
        signal_send(u, sig);

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal