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

#include <uapi/vespera/handels.h>
#include <vespera/realm/realm.h>

Realm::Realm()
    : id(0)
    , capabilities(CAP_NONE)
    , memory_limit(0)
    , max_units(0)
    , unit_count(0)
    , pml4(nullptr)
    , pml4_phys()
    , page_table(nullptr)
    , cwd_path{}
    , unit_list(nullptr)
    , active(false)
    , sched_priority(0)
    , cpu_time_accumulated(0) {
    char buf[50];
    snprintf(buf, sizeof(buf), "realm_%s:%u_lock", name, id);
    lock.init(buf);

    char buf2[100];
    snprintf(buf2, sizeof(buf2), "realm_%s:%u_handle_table_lock", name, id);  // TODO absichern
    memset(&handle_table, 0, sizeof(handle_table));
    handle_table.lock.init(buf2);
}

i64 Realm::init_handle_table() {
    memset(&handle_table, 0, sizeof(HandleTable));
    handle_table.owner_realm = id;
    return SUCCESS_CODE;
}

i64 Realm::add_handle(
    u64 type, void* resource, capability_set caps, bool transferable, void (*destroy)(void*), HandleId* out_h
) {
    SpinlockGuard guard(lock);
    int slot = find_free_slot();
    if (slot < 0) {
        return -ENOMEM;
    }

    set_bit(slot);
    HandleEntry& he = handle_table.entries[slot];
    he.hid = type | (slot & HANDLE_ID_MASK);
    he.type = type;
    he.resource = resource;
    he.capabilities = caps;
    he.refcount = 1;
    he.transferable = transferable;
    he.destroy = destroy;

    *out_h = he.hid;
    return SUCCESS_CODE;
}

i64 Realm::add_handle_with_id(
    HandleId fixed_id, u64 type, void* resource, capability_set caps, bool transferable, void (*destroy)(void*)
) {
    u64 slot = fixed_id & HANDLE_ID_MASK;
    if (slot >= MAX_HANDLES_PER_REALM) {
        return -EBADH;
    }

    SpinlockGuard guard(lock);

    if (test_bit(slot)) {
        return -EBADH;
    }

    set_bit(slot);
    HandleEntry& he = handle_table.entries[slot];
    he.hid = fixed_id;
    he.type = type;
    he.resource = resource;
    he.capabilities = caps;
    he.refcount = 1;
    he.transferable = transferable;
    he.destroy = destroy;

    return SUCCESS_CODE;
}

i64 Realm::setup_standard_handles(TtyDevice* tty_dev) {
    i64 err = add_handle_with_id(HANDLE_STDIN, HANDLE_TYPE_TTY, tty_dev, CAP_READ, false, nullptr);
    if (err != SUCCESS_CODE) return err;

    err = add_handle_with_id(HANDLE_STDOUT, HANDLE_TYPE_TTY, tty_dev, CAP_WRITE, false, nullptr);
    if (err != SUCCESS_CODE) return err;

    err = add_handle_with_id(HANDLE_STDERR, HANDLE_TYPE_TTY, tty_dev, CAP_WRITE, false, nullptr);
    if (err != SUCCESS_CODE) return err;

    return SUCCESS_CODE;
}

HandleEntry* Realm::lookup_handle(HandleId hid) {
    u64 raw = hid & HANDLE_ID_MASK;

    if (raw >= MAX_HANDLES_PER_REALM) return nullptr;
    if (!test_bit(raw)) return nullptr;

    HandleEntry& he = handle_table.entries[raw];
    if (he.hid != hid) return nullptr;
    return &he;
}

void Realm::acquire_handle(HandleId hid) {
    if (auto he = lookup_handle(hid)) {
        __sync_add_and_fetch(&he->refcount, 1);
    }
}

void Realm::release_handle(HandleId hid) {
    HandleEntry* he = lookup_handle(hid);
    if (!he) return;

    if (const u64 v = __sync_sub_and_fetch(&he->refcount, 1); v == 0) {
        SpinlockGuard guard(lock);
        const auto raw = static_cast<u64>(he->hid & HANDLE_ID_MASK);
        if (he->destroy && he->resource) he->destroy(he->resource);
        memset(he, 0, sizeof(HandleEntry));
        clear_bit(raw);
    }
}

TtyDevice* Realm::get_tty_device() const {
    HandleEntry* he = const_cast<Realm*>(this)->lookup_handle(HANDLE_STDIN);
    if (!he || he->type != HANDLE_TYPE_TTY) return nullptr;
    return static_cast<TtyDevice*>(he->resource);
}

void Realm::clear_handle_table() {
    for (usize i = 0; i < MAX_HANDLES_PER_REALM; ++i) {
        if (test_bit(i)) {
            HandleEntry& he = handle_table.entries[i];
            if (he.destroy && he.resource) he.destroy(he.resource);
            clear_bit(i);
            memset(&he, 0, sizeof(HandleEntry));
        }
    }
}

bool Realm::test_bit(usize i) const {
    return (handle_table.bitmap[i >> 3] >> (i & 7)) & 1;
}

void Realm::set_bit(usize i) {
    handle_table.bitmap[i >> 3] |= (1 << (i & 7));
}

void Realm::clear_bit(usize i) {
    handle_table.bitmap[i >> 3] &= ~(1 << (i & 7));
}

int Realm::find_free_slot() const {
    for (usize i = 3; i < MAX_HANDLES_PER_REALM; ++i) {
        if (!test_bit(i)) return static_cast<int>(i);
    }
    return -1;
}
