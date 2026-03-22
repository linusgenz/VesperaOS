// realm.h
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

#ifndef VESPERAOS_KERNEL_REALM_H
#define VESPERAOS_KERNEL_REALM_H

#include <uapi/vespera/capabilities.h>
#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>

#include "../../../kernel/paging/page_table_manager.h"
#include "../../../kernel/tty/tty_device.h"
#include "../types.h"

class Unit;

#define MAX_HANDLES_PER_REALM 4096
#define KERNEL_REALM_SYSTEM 1
#define KERNEL_REALM_DRIVER 2

struct HandleEntry {
    HandleId hid;
    u64 type;
    void *resource;
    capability_set capabilities;
    volatile u64 refcount;
    bool transferable;
    Spinlock lock;

    void (*destroy)(void *);
    void (*acquire)(void*);
};

struct HandleTable {
    HandleEntry entries[MAX_HANDLES_PER_REALM];
    u8 bitmap[MAX_HANDLES_PER_REALM / 8];
    RealmId owner_realm;
    Spinlock lock;
};

class Realm {
   public:
    RealmId id;
    char name[64];
    capability_set capabilities;
    u64 memory_limit;
    u64 max_units;
    u64 unit_count;

    PageTable *pml4;
    phys_addr_t pml4_phys;
    PageTableManager *page_table;

    char cwd_path[256];

    bool exited;

    Unit *unit_list;

    WaitQueue wait_queue;

    HandleTable handle_table;

    Spinlock lock;
    bool active;
    u8 sched_priority;
    u64 cpu_time_accumulated;

    Realm();

    i64 init_handle_table();

    i64 add_handle(
        u64 type, void *resource, capability_set caps, bool transferable, void (*destroy)(void *), void (*acquire)(void*), HandleId *out_h
    );

    i64 add_handle_with_id(
        HandleId fixed_id, u64 type, void *resource, capability_set caps, bool transferable,
        void (*destroy)(void *), void (*acquire)(void*)
    );

    i64 setup_standard_handles(TtyDevice *tty_dev);

    HandleEntry *lookup_handle(HandleId hid);

    void acquire_handle(HandleId hid);

    void release_handle(HandleId hid);
    TtyDevice *get_tty_device() const;

    void clear_handle_table();

   private:
    [[nodiscard]] bool test_bit(usize i) const;
    void set_bit(usize i);
    void clear_bit(usize i);
    [[nodiscard]] int find_free_slot() const;
};

#endif  // VESPERAOS_KERNEL_REALM_H
