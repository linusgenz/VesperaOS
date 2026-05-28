// shm.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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
#ifndef VESPERAOS_SHM_H
#define VESPERAOS_SHM_H

#include <vespera/types.h>
#include <vespera/mm/addr.h>
#include <vespera/sync/spinlock.h>
class Realm;

struct ShmObject {
    char name[64];
    phys_addr_t* pages;
    usize page_count;
    usize size;

    u32 handle_count;
    u32 mapping_count;
    bool unlinked;
    Spinlock lock;

    static ShmObject* create(const char* name, usize initial_size);
    void resize(usize new_size);
    void release_handle();
    void release_mapping();
};

namespace kernel::shm {
    // Syscall Backends
    i64 shm_open(const char* name, int oflag, u32 mode, Realm* current_realm);
    i64 shm_unlink(const char* name);
    i64 shm_truncate(HandleId hid, usize length, Realm* current_realm);
}

#endif  // VESPERAOS_SHM_H
