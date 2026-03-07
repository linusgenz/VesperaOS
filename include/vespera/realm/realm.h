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

#include <vespera/sync/spinlock.h>
#include <vespera/sync/wait_queue.h>

#include "../../../filesystem/vfs/vfs.h"
#include "../../../kernel/paging/page_table_manager.h"
#include "../../../kernel/tty/tty_device.h"
#include "../../../kernel/types/handle.h"
#include "../../../kernel/types/types.h"

class Unit;

class Realm {
public:
    realm_id_t id;
    const char *name;
    capability_set_t capabilities;
    uint64_t memory_limit;
    uint64_t max_units;
    uint64_t unit_count;

    PageTable *pml4;
    phys_addr_t pml4_phys;
    PageTableManager *page_table;

    char cwd_path[256];

    Unit *unit_list;

    WaitQueue wait_queue;

    handle_table_t handle_table;

    Spinlock lock;
    bool active;
    uint8_t sched_priority;
    uint64_t cpu_time_accumulated;

    Realm();

    error_code_t init_handle_table();

    error_code_t add_handle(uint64_t type, void *resource,
                         capability_set_t caps, bool transferable,
                         void (*destroy)(void *), handle_id_t *out_h);

    error_code_t add_handle_with_id(handle_id_t fixed_id, uint64_t type, void *resource,
                                 capability_set_t caps, bool transferable,
                                 void (*destroy)(void *));

    error_code_t setup_standard_handles(TtyDevice *tty_dev);

    handle_entry_t* lookup_handle(handle_id_t hid);

    void acquire_handle(handle_id_t hid);

    void release_handle(handle_id_t hid);

    void clear_handle_table();

private:
    [[nodiscard]] bool test_bit(size_t i) const;
    void set_bit(size_t i);
    void clear_bit(size_t i);
    [[nodiscard]] int find_free_slot() const;
};

#endif //VESPERAOS_REALM_H
