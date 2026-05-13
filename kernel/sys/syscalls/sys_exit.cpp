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

#include <scheduling/scheduler_types.h>
#include <vespera/cpu/cpu_manager.h>
#include <vespera/kerrno.h>
#include <vespera/log.h>
#include <vespera/realm/exit_code_table.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

namespace syscalls::internal {
    i64 sys_exit(u64 code, u64, u64, u64, u64, u64) {
        u8 cpu_id = cpu_manager::get_current_cpu_id();
        kernel::scheduling::cpu_scheduler::CpuScheduler* cpu = kernel::scheduling::get_cpu_data(cpu_id);
        Unit* current = kernel::scheduling::get_current_unit();
        if (!current) {
            kernel::SystemManager::system_panic("Attempt to exit a unit that no longer exists", -KENOUNIT);
        }

        if (Realm* realm = current->parent) {
            SpinlockGuard g(realm->lock);
            if (realm->unit_count == 1) {
                ExitCodeTable::store(realm->id, static_cast<int>(code));
                realm->exit_code = code;
                realm->exited = true;
                realm->wait_queue.wake_all();
            }
        }

        current->exit_code = static_cast<int>(code);
        current->state = UnitState::Terminated;
        cpu->reaper.enqueue(current);

        cpu->current_unit = nullptr;

        kernel::scheduling::yield();

        for (;;) {
            asm volatile("hlt");
        }
    }
}  // namespace syscalls::internal
