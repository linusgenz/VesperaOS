// realm.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 28.11.25.
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

#include <vespera/realm/realm.h>

Realm::Realm()
    : id(0)
    , name(nullptr)
    , capabilities(CAP_NONE)
    , memory_limit(0)
    , max_units(0)
    , unit_count(0)
    , pml4(nullptr)
    , pml4_phys()
    , page_table(nullptr)
    , unit_list(nullptr)
    , active(false)
    , sched_priority(0)
    , cpu_time_accumulated(0) {
    const auto path = "/";
    memcpy(cwd_path, path, strlen(path));
    char buf[50];
    snprintf(buf, sizeof(buf), "realm_%s:%u_lock", name, id);
    lock.init(buf);

    char buf2[100];
    snprintf(buf2, sizeof(buf), "realm_%s:%u_handle_table_lock", name, id);  // TODO absichern
    memset(&handle_table, 0, sizeof(handle_table));
    handle_table.lock.init(buf2);
}

error_code_t Realm::init_handle_table() {
    memset(&handle_table, 0, sizeof(handle_table_t));
    handle_table.owner_realm = id;
    return MOD_SUCCESS;
}

error_code_t Realm::add_handle(
    uint64_t type, void* resource, capability_set_t caps, bool transferable, void (*destroy)(void*), handle_id_t* out_h
) {
    SpinlockGuard guard(lock);
    int slot = find_free_slot();
    if (slot < 0) {
        return MOD_ERR_OUT_OF_MEMORY;
    }

    set_bit(slot);
    handle_entry_t& he = handle_table.entries[slot];
    he.hid = type | static_cast<handle_id_t>(slot & HANDLE_ID_MASK);
    he.type = type;
    he.resource = resource;
    he.capabilities = caps;
    he.refcount = 1;
    he.transferable = transferable;
    he.destroy = destroy;

    *out_h = he.hid;
    return MOD_SUCCESS;
}

error_code_t Realm::add_handle_with_id(
    handle_id_t fixed_id, uint64_t type, void* resource, capability_set_t caps, bool transferable, void (*destroy)(void*)
) {
    uint64_t slot = fixed_id & HANDLE_ID_MASK;
    if (slot >= MAX_HANDLES_PER_REALM) {
        return MOD_ERR_INVALID_HANDLE;
    }

    SpinlockGuard guard(lock);

    if (test_bit(slot)) {
        return MOD_ERR_INVALID_HANDLE;
    }

    set_bit(slot);
    handle_entry_t& he = handle_table.entries[slot];
    he.hid = fixed_id;
    he.type = type;
    he.resource = resource;
    he.capabilities = caps;
    he.refcount = 1;
    he.transferable = transferable;
    he.destroy = destroy;

    return MOD_SUCCESS;
}

error_code_t Realm::setup_standard_handles(TtyDevice* tty_dev) {
    error_code_t err = add_handle_with_id(HANDLE_STDIN, HANDLE_TYPE_TTY, tty_dev, CAP_READ, false, nullptr);
    if (err != MOD_SUCCESS) return err;

    err = add_handle_with_id(HANDLE_STDOUT, HANDLE_TYPE_TTY, tty_dev, CAP_WRITE, false, nullptr);
    if (err != MOD_SUCCESS) return err;

    err = add_handle_with_id(HANDLE_STDERR, HANDLE_TYPE_TTY, tty_dev, CAP_WRITE, false, nullptr);
    if (err != MOD_SUCCESS) return err;

    return MOD_SUCCESS;
}

handle_entry_t* Realm::lookup_handle(handle_id_t hid) {
    uint64_t raw = hid & HANDLE_ID_MASK;

    if (raw >= MAX_HANDLES_PER_REALM) return nullptr;
    if (!test_bit(raw)) return nullptr;

    handle_entry_t& he = handle_table.entries[raw];
    if (he.hid != hid) return nullptr;
    return &he;
}

void Realm::acquire_handle(handle_id_t hid) {
    if (auto he = lookup_handle(hid)) {
        __sync_add_and_fetch(&he->refcount, 1);
    }
}

void Realm::release_handle(handle_id_t hid) {
    handle_entry_t* he = lookup_handle(hid);
    if (!he) return;

    if (const uint64_t v = __sync_sub_and_fetch(&he->refcount, 1); v == 0) {
        SpinlockGuard guard(lock);
        const auto raw = static_cast<uint64_t>(he->hid & HANDLE_ID_MASK);
        if (he->destroy && he->resource) he->destroy(he->resource);
        memset(he, 0, sizeof(handle_entry_t));
        clear_bit(raw);
    }
}

void Realm::clear_handle_table() {
    for (size_t i = 0; i < MAX_HANDLES_PER_REALM; ++i) {
        if (test_bit(i)) {
            handle_entry_t& he = handle_table.entries[i];
            if (he.destroy && he.resource) he.destroy(he.resource);
            clear_bit(i);
            memset(&he, 0, sizeof(handle_entry_t));
        }
    }
}

bool Realm::test_bit(size_t i) const {
    return (handle_table.bitmap[i >> 3] >> (i & 7)) & 1;
}

void Realm::set_bit(size_t i) {
    handle_table.bitmap[i >> 3] |= (1 << (i & 7));
}

void Realm::clear_bit(size_t i) {
    handle_table.bitmap[i >> 3] &= ~(1 << (i & 7));
}

int Realm::find_free_slot() const {
    for (size_t i = 3; i < MAX_HANDLES_PER_REALM; ++i) {
        if (!test_bit(i)) return static_cast<int>(i);
    }
    return -1;
}
