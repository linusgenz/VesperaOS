// unit_termination.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
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

#include "unit_termination.h"

#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/realm/exit_code_table.h>
#include <vespera/scheduling.h>

#include "../../kernel/cpu/cpu_manager.h"

static void do_terminate_unit(Unit* unit, Signal fault_sig) {
    unit->exit_code = -static_cast<i32>(fault_sig);
    unit->state = UnitState::Terminated;

    const u8 cpu_id = unit->cpu_id;
    auto* cpu = kernel::scheduling::get_cpu_data(cpu_id);
    cpu->ready_queue.remove(unit);
    cpu->blocked_queue.remove(unit);

    cpu->reaper.enqueue(unit);
}

namespace kernel::scheduling {
    [[noreturn]] void kill_current_realm(Signal fault_sig, const char* fault_name) {
        asm volatile("cli");

        u8 cpu_id = cpu_manager::get_current_cpu_id();
        auto* cpu = get_cpu_data(cpu_id);

        Unit* dying = cpu->current_unit;
        Realm* realm = dying ? RealmManager::get(dying->rid) : nullptr;

        if (!realm) {
            Log::error("Fatal fault without realm context: %s", fault_name);
            SystemManager::system_panic("Uncontexted fault", -KENOCTXFLT);
        }

        realm->exit_code = -static_cast<i32>(fault_sig);
        ExitCodeTable::store(realm->id, realm->exit_code);

        // future TODO: build core dumper

        {
            SpinlockGuard guard(cpu->lock);

            cpu->current_unit = nullptr;

            Unit* u = realm->unit_list;
            while (u) {
                Unit* next = u->next;
                do_terminate_unit(u, fault_sig);
                u = next;
            }
        }

        realm->wait_queue.wake_all();
        realm->unit_count = 0;

        yield();
        __builtin_unreachable();
    }
}  // namespace kernel::scheduling