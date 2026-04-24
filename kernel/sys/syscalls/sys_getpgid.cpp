// sys_getpgid.cpp
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
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_getpgid(u64 arg0, u64, u64, u64, u64, u64) {
        const Unit* caller = kernel::scheduling::get_current_unit();
        if (!caller) return -ESRCH;

        const RealmId target_rid = arg0 ? arg0 : caller->rid;

        const Realm* target = RealmManager::get(target_rid);
        if (!target) return -ESRCH;

        return static_cast<i64>(target->pgid);
    }
}  // namespace syscalls::internal
