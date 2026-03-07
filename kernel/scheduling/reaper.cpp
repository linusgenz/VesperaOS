/**
 * @file reaper.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 07.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#include "reaper.h"

#include <vespera/log.h>
#include <vespera/scheduling.h>
#include <vespera/time.h>
#include "../cpu/cpu_manager.h"
#include "../units/unit_manager.h"

[[noreturn]] void reaper_unit(void* arg) {
    while (true) {
        asm volatile("cli");
        uint8_t cpu_id = cpu_manager::get_current_cpu_id();
        if (auto* cpu = kernel::scheduling::get_cpu_data(cpu_id); !cpu->reaper.empty()) {
            cpu->reaper.reap();
        }
        asm volatile("sti");
        kernel::time::sleep_ms(1000);
        // kernel::scheduling::yield();
    }
}

void Reaper::enqueue(Unit* unit) {
    pending_.push(unit);
}

void Reaper::reap() {
    Unit* unit = pending_.pop();
    while (unit) {
        Unit* next = unit->next;
        UnitManager::destroy(unit->id);
        unit = next;
    }
}

bool Reaper::empty() const {
    return pending_.empty();
}
