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

#ifndef VESPERAOS_REALM_H
#define VESPERAOS_REALM_H

#include <cstdint>

#include "../../../filesystem/vfs/vfs.h"
#include "../../../kernel/types/types.h"
#include <kernel/sync/spinlock.h>
#include "../../../kernel/types/handle.h"
#include "../../../kernel/tty/tty_device.h"
#include "../../../kernel/paging/page_table_manager.h"
#include <kernel/sync/wait_queue.h>

class Unit;

class Realm {
public:
    RealmID id;
    const char *name;
    CapabilitySet capabilities;
    uint64_t memory_limit;
    uint64_t max_units;
    uint64_t unit_count;

    PageTable *pml4;
    PageTableManager *page_table;

    char cwd_path[256];

    Unit *unit_list;

    wait_queue_t wait_queue;

    handle_table_t handle_table;

    const char **envp;

    spinlock_t lock;
    bool active;
    uint8_t sched_priority;
    uint64_t cpu_time_accumulated;

    Realm();

    ErrorCode init_handle_table();

    ErrorCode add_handle(uint64_t type, void *resource,
                         CapabilitySet caps, bool transferable,
                         void (*destroy)(void *), HandleID *out_h);

    ErrorCode add_handle_with_id(HandleID fixed_id, uint64_t type, void *resource,
                                 CapabilitySet caps, bool transferable,
                                 void (*destroy)(void *));

    ErrorCode setup_standard_handles(TTYDevice *tty_dev);

    handle_entry_t* lookup_handle(HandleID hid);

    void acquire_handle(HandleID hid);

    void release_handle(HandleID hid);

    void clear_handle_table();

private:
    bool test_bit(size_t i) const;
    void set_bit(size_t i);
    void clear_bit(size_t i);
    int find_free_slot() const;
};

#endif //VESPERAOS_REALM_H
