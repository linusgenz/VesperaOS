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

#include <kernel/sync/spinlock.h>

#include "../types/types.h"
#include "unit.h"

using unit_entry_t = void (*)(void*);

class UnitManager {
   public:
    static void initialize();
    static bool is_initialized();
    static Unit* create(realm_id_t realm_id, unit_entry_t entry_point, void* arg, const UnitConfig* cfg);
    static Unit* get(unit_id_t id);
    static bool destroy(unit_id_t id);
    static void list();

    static ssize_t get_status(void* manager_ref, void* buffer, size_t size, size_t offset);

   private:
    static constexpr size_t MAX_UNITS = 256;
    static Unit units_[MAX_UNITS];
    static Spinlock global_lock_;
    static unit_id_t next_id_;
    static bool initialized_;

    static unit_id_t allocate_id();

    static void setup_kernel_unit_stack(Unit* u);
    static void setup_user_unit_stack(Unit* u);
};

uintptr_t setup_user_args_and_env(Unit* u, const char** argv, const char** envp);

#endif  // VESPERAOS_UNIT_MANAGER_H
