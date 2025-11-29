// sys_exit.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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

#include "cstdint"
#include "../../../include/log.h"
#include "../../cpu/cpu_manager.h"
#include <kernel/scheduling.h>

#include <kernel/realm/realm_manager.h>
#include <kernel/system/system_manager.h>
#include "../../units/unit_manager.h"
#include "../../utils/panic.h"

namespace syscalls::internal {
    int64_t sys_exit(uint64_t code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        uint8_t cpu_id = CPUManager::get_current_cpu_id();
        Unit* current = kernel::scheduling::get_current_unit();
        if (!current) {
            kernel::SystemManager::system_panic("Attempt to exit a unit that no longer exists", -KENOUNIT);
        }

        current->exit_code = (int) code;
        current->state = UNIT_TERMINATED;
        current->active = false;
        current->handle_count = 0;

        UnitManager::destroy(current->id);

        // Realm-Bookkeeping
        if (Realm* realm = RealmManager::get(current->rid)) {
            realm->unit_count--;
            if (realm->unit_count == 0) {
                RealmManager::destroy(realm->id);
            }
        }

        kernel::scheduling::cpu_scheduler::cpu_scheduler_t* cpu =
            kernel::scheduling::get_cpu_data(cpu_id);
        cpu->current_unit = nullptr;

        kernel::scheduling::yield();

        for (;;) { asm volatile("hlt"); }
    }
}
