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

#ifndef VESPERAOS_VESPERA_UNIT_UNIT_MANAGER_H
#define VESPERAOS_VESPERA_UNIT_UNIT_MANAGER_H

#include <klib/result.h>
#include <vespera/types.h>

struct UnitConfig;
class Realm;
class Unit;
class Spinlock;

using unit_entry_t = void (*)(void*);

// UnitManager — creates, owns, and destroys all Unit instances.
class UnitManager {
   public:
    static void initialize();
    static bool is_initialized();

    // Create a unit in the given realm and add it to the scheduler if
    // cfg->auto_schedule is set. Returns nullptr on failure.
    static Unit* create(RealmId realm_id, unit_entry_t entry, void* arg, const UnitConfig* cfg);

    static Unit* get(UnitId id);
    static bool destroy(UnitId id);
    static void reap(Unit* u);
    static Result<int> join(UnitId id);
    static void list();

    static Result<usize> get_status(void* manager_ref, void* buffer, usize size, usize offset);

   private:
    static constexpr usize MAX_UNITS = 256;

    static Unit units_[MAX_UNITS];
    static Spinlock global_lock_;
    static UnitId next_id_;
    static bool initialized_;

    static UnitId allocate_id();

    // Find an inactive slot and zero-initialize it.
    // Returns nullptr if the pool is exhausted.
    static Unit* alloc_slot(Realm* realm, const char* name, unit_entry_t entry, void* arg, const UnitConfig* cfg);

    // Allocate and map the kernel stack. For user units, also
    // allocates a user stack slot from the realm's AddressSpace and maps
    // the physical pages into the user page table.
    // Returns false and marks the unit inactive on failure.
    static bool setup_stacks(Unit* u, Realm* realm, const UnitConfig* cfg);

    // Push argv/envp onto the user stack (main unit only), then
    // initialize the cpu_ctx (kernel or user segment selectors, rip, rsp).
    static void setup_context(Unit* u, const UnitConfig* cfg);

    // Attach initial handles from cfg->initial_handles to the
    // unit's handle set and increment the realm HandleTable refcount.
    static void attach_initial_handles(Unit* u, Realm* realm, const UnitConfig* cfg);

    // Add the unit to the scheduler run queue, link it into the
    // realm's unit list, and register it with the RealmFs.
    static void register_unit(Unit* u, Realm* realm, const UnitConfig* cfg);
};

uptr setup_user_args_and_env(Unit* u, const char** argv, const char** envp);

#endif  // VESPERAOS_VESPERA_UNIT_UNIT_MANAGER_H
