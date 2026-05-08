// handle_table.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 08.05.26.
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


#ifndef VESPERAOS_KERNEL_REALM_HANDLE_TABLE_H
#define VESPERAOS_KERNEL_REALM_HANDLE_TABLE_H

#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/handles.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include "klib/result.h"

class TtyDevice;
class Channel;

struct HandleEntry {
    HandleId        hid;
    u64             type;
    void*           resource;
    capability_set  capabilities;
    volatile u64    refcount;
    bool            transferable;
    void (*destroy)(void*);
    void (*acquire)(void*);
};

constexpr usize MAX_HANDLES_PER_REALM = 4096;


class HandleTable {
public:
    HandleTable() = default;

    HandleTable(const HandleTable&) = delete;
    HandleTable& operator=(const HandleTable&) = delete;

    void init(RealmId owner);

    [[nodiscard]] Result<HandleId> add(
        u64             type,
        void*           resource,
        capability_set  caps,
        bool            transferable,
        void          (*destroy)(void*),
        void          (*acquire)(void*)
    );

    [[nodiscard]] VoidResult add_at(
        HandleId        fixed_id,
        u64             type,
        void*           resource,
        capability_set  caps,
        bool            transferable,
        void          (*destroy)(void*),
        void          (*acquire)(void*)
    );

    [[nodiscard]] VoidResult setup_standard_handles(TtyDevice* tty_dev);

    [[nodiscard]] HandleEntry* lookup(HandleId hid);

    void acquire(HandleId hid);
    void release(HandleId hid);

    void clear();

    [[nodiscard]] RealmId owner() const { return owner_; }

private:
    HandleEntry entries_[MAX_HANDLES_PER_REALM]{};
    u8          bitmap_[MAX_HANDLES_PER_REALM / 8]{};
    RealmId     owner_{0};
    Spinlock    lock_;

    [[nodiscard]] bool test_bit(usize i) const;
    void set_bit(usize i);
    void clear_bit(usize i);
    [[nodiscard]] int find_free_slot() const;
};

#endif // VESPERAOS_KERNEL_REALM_HANDLE_TABLE_H

