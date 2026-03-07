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

#include "../../../filesystem/vfs/vfs.h"
#include "../../../kernel/paging/page_table_manager.h"
#include "../../../kernel/tty/tty_device.h"
#include "../types.h"

class Unit;

#define MAX_HANDLES_PER_REALM 4096
#define KERNEL_REALM_SYSTEM 1
#define KERNEL_REALM_DRIVER 2

struct HandleEntry {
    HandleId hid;
    uint64_t type;
    void *resource;
    capability_set capabilities;
    volatile uint64_t refcount;
    bool transferable;
    Spinlock lock;

    void (*destroy)(void *);
};

struct HandleTable {
    HandleEntry entries[MAX_HANDLES_PER_REALM];
    uint8_t bitmap[MAX_HANDLES_PER_REALM / 8];
    RealmId owner_realm;
    Spinlock lock;
};

class Realm {
   public:
    RealmId id;
    const char *name;
    capability_set capabilities;
    uint64_t memory_limit;
    uint64_t max_units;
    uint64_t unit_count;

    PageTable *pml4;
    phys_addr_t pml4_phys;
    PageTableManager *page_table;

    char cwd_path[256];

    Unit *unit_list;

    WaitQueue wait_queue;

    HandleTable handle_table;

    Spinlock lock;
    bool active;
    uint8_t sched_priority;
    uint64_t cpu_time_accumulated;

    Realm();

    int64_t init_handle_table();

    int64_t add_handle(
        uint64_t type, void *resource, capability_set caps, bool transferable, void (*destroy)(void *), HandleId *out_h
    );

    int64_t add_handle_with_id(
        HandleId fixed_id, uint64_t type, void *resource, capability_set caps, bool transferable,
        void (*destroy)(void *)
    );

    int64_t setup_standard_handles(TtyDevice *tty_dev);

    HandleEntry *lookup_handle(HandleId hid);

    void acquire_handle(HandleId hid);

    void release_handle(HandleId hid);

    void clear_handle_table();

   private:
    [[nodiscard]] bool test_bit(size_t i) const;
    void set_bit(size_t i);
    void clear_bit(size_t i);
    [[nodiscard]] int find_free_slot() const;
};

#endif  // VESPERAOS_KERNEL_REALM_H
