// sys_spawn.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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
#include <vespera/tty/tty.h>
#include <vespera_errno.h>

#include "../../exec/elf.h"
#include "../../units/unit_manager.h"

namespace syscalls::internal {
    i64 sys_spawn(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        auto user_path = reinterpret_cast<const char*>(arg0);
        const auto argc = static_cast<u32>(arg1);
        auto argv = reinterpret_cast<const char**>(arg2);
        auto envp = reinterpret_cast<const char**>(arg3);
        envp = nullptr;

        if (!user_path) return -EINVAL;

        RealmConfig cfg = {
            .name = user_path,  // TODO change name (clashing with fs)
            .capabilities = CAP_RW | CAP_DEVICE_ACCESS,
            .is_user = true
        };

        Realm* new_realm = RealmManager::create(&cfg);
        if (!new_realm) return -ENOMEM;
        TtyDevice* tty_dev = kernel::tty::tty_devices[0];
        new_realm->setup_standard_handles(tty_dev);

        const ElfLoader::LoadResult elf = ElfLoader::load(user_path, 0x500000, new_realm);

        if (!elf.success) {
            RealmManager::destroy(new_realm->id);
            return -ENOEXEC;
        }

        UnitConfig ucfg = {
            .name = "main_unit",
            .cpu_id = 6,
            .priority = 5,
            .is_user = true,
            .auto_schedule = false,
        };
        Unit* u = UnitManager::create(
            new_realm->id, reinterpret_cast<unit_entry_t>(elf.entry_point), reinterpret_cast<void*>(arg1), &ucfg
        );
        if (!u) {
            RealmManager::destroy(new_realm->id);
            return -EFAULT;
        }

        /* uptr user_sp = SetupUserArgsAndEnv(u, argv, envp);
         if (user_sp == 0) {
             // cleanup
             UnitManager::destroy(u->id);
             RealmManager::destroy(new_realm->id);
             return -EFAULT;
         }

         u->context.user_stack_pointer = reinterpret_cast<void*>(user_sp);*/
        kernel::scheduling::add_unit(u);

        return new_realm->id;
    }
}  // namespace syscalls::internal
