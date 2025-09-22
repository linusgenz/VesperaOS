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

#include <cstdint>
#include <errno.h>

#include "../../exec/elf.h"
#include "../../realm/realm_manager.h"
#include "../../units/unit_manager.h"

namespace syscalls::internal {
    int64_t sys_spawn(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        auto user_path = reinterpret_cast<const char*>(arg0);
        auto argc = static_cast<uint32_t>(arg1);
        auto argv = reinterpret_cast<const char**>(arg2);

        if (!user_path) return -EINVAL;

        RealmConfig cfg = {
            .name = user_path,
            .capabilities = CAP_RW | CAP_DEVICE_ACCESS
        };

        Realm* new_realm = RealmManager::create(&cfg);
        if (!new_realm) return -ENOMEM;
        auto console_dev = new ConsoleDevice();
        new_realm->setup_standard_handles(console_dev);

        ElfLoader loader;
        ElfLoader::ElfLoadResult elf = loader.load_elf_binary(user_path, 0x500000);
        if (!elf.success) {
            RealmManager::destroy(new_realm->id);
            return -ENOEXEC;
        }

        UnitConfig ucfg = {
            .name = "unnamed_unit",
            .cpu_id = 0,
            .priority = 5,
            .is_user = true,
        };
        Unit* u = UnitManager::create(new_realm->id, (void*)elf.entry_point, (void*)arg1, &ucfg);
        if (!u) {
            RealmManager::destroy(new_realm->id);
            return -EFAULT;
        }

        u->context.regs.rdi = static_cast<uint64_t>(argc);
        u->context.regs.rsi = reinterpret_cast<uint64_t>(argv);
        u->context.regs.rdx = 0;
        u->context.regs.rcx = 0;
        u->context.regs.r8  = 0;
        u->context.regs.r9  = 0;

        return new_realm->id;
    }
}
