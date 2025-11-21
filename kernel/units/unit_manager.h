// unit_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_UNIT_MANAGER_H
#define VESPERAOS_UNIT_MANAGER_H

#include "unit.h"
#include "../sync/spinlock.h"
#include "../types/types.h"

class UnitManager {
public:
    static void initialize();
    static bool is_initialized();
    static Unit* create(RealmID realm_id, void* entry_point, void* arg, const UnitConfig* cfg);
    static Unit* get(UnitID id);
    static bool destroy(UnitID id);
    static void list();

private:
    static constexpr size_t MAX_UNITS = 256;
    static Unit units[MAX_UNITS];
    static spinlock_t global_lock;
    static UnitID next_id;

    static UnitID allocate_id();

    static void setup_kernel_unit_stack(Unit* u);
    static void setup_user_unit_stack(Unit *u);
};

uintptr_t SetupUserArgsAndEnv(Unit *u, const char **argv, const char **envp);

#endif //VESPERAOS_UNIT_MANAGER_H