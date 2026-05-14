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

#include <cpu/cpu_manager.h>
#include <realm/realm.h>
#include <scheduling/scheduler_types.h>
#include <units/unit.h>
#include <vespera/kerrno.h>
#include <vespera/log.h>
#include <vespera/realm/exit_code_table.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>
#include <vespera/time.h>

static void do_terminate_unit(Unit* unit, Signal fault_sig) {
    unit->exit_code = -static_cast<i32>(fault_sig);
    unit->state = UnitState::Terminated;

    if (unit->run_start_ns != 0) {
        unit->cpu_time_ns += kernel::time::get_uptime_ns() - unit->run_start_ns;
        unit->run_start_ns = 0;
    }

    const u8 cpu_id = unit->cpu_id;
    auto* cpu = kernel::scheduling::get_cpu_data(cpu_id);
    cpu->ready_queue.remove(unit);
    cpu->blocked_queue.remove(unit);

    cpu->reaper.enqueue(unit);
}

namespace kernel::scheduling {
    [[noreturn]] static void kill_realm_internal(Realm* realm, Signal sig, const char* reason) {
        if (!realm) {
            Log::error("kill_realm_internal: null realm (%s)", reason);
            SystemManager::system_panic("Null realm", -KENOCTXFLT);
        }

        realm->exit_code = static_cast<i32>(sig) & 0x7f;
        ExitCodeTable::store(realm->id, realm->exit_code);

        // TODO: core dump

        {
            const u8 cpu_id = cpu_manager::get_current_cpu_id();
            auto* cpu = get_cpu_data(cpu_id);
            SpinlockGuard guard(cpu->lock);

            if (cpu->current_unit && cpu->current_unit->rid == realm->id) {
                cpu->current_unit = nullptr;
            }

            Unit* u = realm->unit_list;
            while (u) {
                Unit* next = u->next;
                do_terminate_unit(u, sig);
                u = next;
            }
        }

        realm->wait_queue.wake_all();
        realm->unit_count = 0;

        yield();
        __builtin_unreachable();
    }

    [[noreturn]] void kill_current_realm(Signal sig, const char* reason) {
        asm volatile("cli");

        u8 cpu_id = cpu_manager::get_current_cpu_id();
        auto* cpu = get_cpu_data(cpu_id);

        Unit* current = cpu->current_unit;
        Realm* realm = current ? current->parent : nullptr;

        if (!realm) {
            Log::error("Fatal fault without realm context: %s", reason);
            SystemManager::system_panic("Uncontexted fault", -KENOCTXFLT);
        }

        kill_realm_internal(realm, sig, reason);
    }

    i64 kill_realm_by_id(u64 rid, Signal sig) {
        Realm* realm = RealmManager::get(rid);
        if (!realm) return -ESRCH;

        u8 cpu_id = cpu_manager::get_current_cpu_id();
        auto* cpu = get_cpu_data(cpu_id);

        Unit* current = cpu->current_unit;

        if (current && current->rid == rid) {
            kill_realm_internal(realm, sig, "self kill");
        }

        {
            SpinlockGuard guard(cpu->lock);

            Unit* u = realm->unit_list;
            while (u) {
                Unit* next = u->next;
                do_terminate_unit(u, sig);
                u = next;
            }
        }

        realm->exit_code = static_cast<i32>(sig) & 0x7f;
        ExitCodeTable::store(realm->id, realm->exit_code);

        realm->wait_queue.wake_all();
        realm->unit_count = 0;

        return SUCCESS_CODE;
    }
}  // namespace kernel::scheduling