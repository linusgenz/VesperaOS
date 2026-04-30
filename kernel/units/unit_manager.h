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

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>
#include <vespera/unit_config.h>

#include "unit.h"

using unit_entry_t = void (*)(void*);

class UnitManager {
   public:
    static void initialize();
    static bool is_initialized();
    static Unit* create(RealmId realm_id, unit_entry_t entry_point, void* arg, const UnitConfig* cfg);
    static Unit* get(UnitId id);
    static bool destroy(UnitId id);
    static void list();

    static isize get_status(void* manager_ref, void* buffer, usize size, usize offset);

   private:
    static constexpr usize MAX_UNITS = 256;
    static Unit units_[MAX_UNITS];
    static Spinlock global_lock_;
    static UnitId next_id_;
    static bool initialized_;

    static UnitId allocate_id();

    static void setup_kernel_unit_stack(Unit* u);
    static void setup_user_unit_stack(Unit* u);
};

uptr setup_user_args_and_env(Unit* u, const char** argv, const char** envp);

#endif  // VESPERAOS_UNIT_MANAGER_H
