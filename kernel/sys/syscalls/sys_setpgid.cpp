// sys_setpgid.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.04.26.
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

#include <vespera/realm/realm.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_setpgid(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const Unit* caller = kernel::scheduling::get_current_unit();
        if (!caller) return -ESRCH;

        Realm* self = caller->parent;
        if (!self) return -ESRCH;

        const RealmId target_rid  = arg0 ? arg0 : self->id;
        const RealmId desired_pgid = arg1 ? arg1 : target_rid;

        Realm* target = RealmManager::get(target_rid);
        if (!target) return -ESRCH;

        // Session leaders may not change pgid.
        if (target->sid == target->id) return -EPERM;

        if (target->sid != self->sid) return -EPERM;

        if (desired_pgid != target->id) {
            bool found = false;
            for (usize i = 0; i < RealmManager::MAX_REALMS; i++) {
                const Realm* r = RealmManager::get(i + 1);
                if (r && r->active && r->sid == self->sid && r->pgid == desired_pgid) {
                    found = true;
                    break;
                }
            }
            if (!found) return -EPERM;
        }

        target->pgid = desired_pgid;
        return 0;
    }
}  // namespace syscalls::internal
