// sys_unit_spawn.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.04.26.
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

#include <units/unit.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    constexpr size_t MIN_STACK_SIZE = static_cast<usize>(16) * 1024;
    i64 sys_unit_spawn(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64) {
        const u64 entry = arg1;
        const u64 arg_ptr = arg2;
        const u64 stack_sz = arg3;
        const u64 flags = arg4;

        // flags, for future use
        if (flags != 0) return -EINVAL;
        if (entry == 0) return -EINVAL;
        if ((stack_sz < MIN_STACK_SIZE && stack_sz != 0) || stack_sz > DEFAULT_UNIT_STACK_SIZE) return -EINVAL;

        const Unit* caller = kernel::scheduling::get_current_unit();
        if (!caller) return -EINVAL;

        const RealmId target_rid = (arg0 == 0) ? caller->rid : arg0;

        // adding cross-realm spawning later
        if (target_rid != caller->rid) return -EACCES;

        Realm* realm = RealmManager::get(target_rid);
        if (!realm || !realm->active) return -ECHILD;

        const UnitConfig ucfg = {
            .name = "unit",  // generic name, maybe expose later
            .cpu_id = 5,
            .priority = caller->priority,
            .stack_size = DEFAULT_UNIT_STACK_SIZE,
            .initial_handles = nullptr,
            .initial_handle_count = 0,
            .is_user = true,
            .is_main_unit = false,
            .user_stack_size = stack_sz,
            .auto_schedule = false,
            .argv = nullptr,
            .envp = nullptr,
        };

        Unit* u = UnitManager::create(
            target_rid, reinterpret_cast<unit_entry_t>(entry), reinterpret_cast<void*>(arg_ptr), &ucfg
        );

        if (!u) return -ENOMEM;

        kernel::scheduling::add_unit(u);

        return static_cast<i64>(u->id);
    }

}  // namespace syscalls::internal