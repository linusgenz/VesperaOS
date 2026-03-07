// handle.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 20.09.25.
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

#ifndef VESPERAOS_HANDLE_H
#define VESPERAOS_HANDLE_H

typedef long int ssize_t;

#define MAX_UNIT_HANDLE_SLOTS 64

#include <stdint.h>
#include <vespera/sync/spinlock.h>

#include "../types/types.h"

typedef struct HandleEntry {
    handle_id_t hid;
    uint64_t type;
    void *resource;
    capability_set_t capabilities;
    volatile uint64_t refcount;
    bool transferable;
    Spinlock lock;

    void (*destroy)(void *);
} handle_entry_t;

typedef struct HandleTable {
    handle_entry_t entries[MAX_HANDLES_PER_REALM];
    uint8_t bitmap[MAX_HANDLES_PER_REALM / 8];
    realm_id_t owner_realm;
    Spinlock lock;
} handle_table_t;

typedef struct UnitHandleTable {
    handle_id_t slots[MAX_UNIT_HANDLE_SLOTS];
    uint64_t count;
    Spinlock lock;
} unit_handle_table_t;

#endif  // VESPERAOS_HANDLE_H
