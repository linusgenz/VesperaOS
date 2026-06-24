// handle_table->cpp
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

#include "handle_table.h"

#include <filesystem/vfs_handle.h>
#include <klib/string.h>
#include <vespera/ipc/channel.h>
#include <vespera/scheduling.h>
#include <vespera/sync/spinlock.h>

#include "../tty/tty_device.h"

void HandleTable::init(const RealmId owner) {
    memset(entries_, 0, sizeof(entries_));
    memset(bitmap_,  0, sizeof(bitmap_));
    owner_ = owner;

    char buf[64];
    snprintf(buf, sizeof(buf), "htable_%u_lock", owner);
    lock_.init(buf);
}

// Opens a device path via VFS and installs it as a stdio handle in dst's handle table.
// caps controls read/write access on the handle.
Result<void> HandleTable::install_stdio_handle(HandleId hid, const char* dev_path, capability_set caps) {
    auto open_res = VFS::open(dev_path);
    if (open_res.is_err()) return open_res.error();

    VfsNode* node = open_res.unwrap();
    VfsHandle* vh = new VfsHandle(node, 0, caps);
    if (!vh) {
        VFS::close(node);
        return Error::NoMem;
    }

    if (lookup(hid)) release(hid);

    auto add_res = add_at(
        hid,
        HANDLE_TYPE_DEVICE,
        vh,
        caps | CAP_DEVICE_ACCESS,
        /*transferable=*/false,
        VfsHandle::destroy,
        VfsHandle::acquire
    );
    if (add_res.is_err()) {
        delete vh;
        return add_res.error();
    }

    return Result<void>::ok();
}

// Sets up stdin/stdout/stderr for a realm, pointing all three at dev_path.
VoidResult HandleTable::setup_stdio(const char* dev_path) {
    TRY_VOID(install_stdio_handle(HANDLE_STDIN, dev_path, CAP_READ));
    TRY_VOID(install_stdio_handle(HANDLE_STDOUT, dev_path, CAP_WRITE));
    TRY_VOID(install_stdio_handle(HANDLE_STDERR, dev_path, CAP_WRITE));
    return VoidResult::ok();
}

VoidResult HandleTable::setup_vbus() {
    ChannelEndpoint* ep = ChannelEndpoint::create(8192, /*read=*/true, /*write=*/true);
    if (!ep) {
        return VoidResult::err(Error::NoMem);
    }

    return add_at(
        HANDLE_VBUS, HANDLE_TYPE_CHANNEL, ep,
        CAP_READ | CAP_WRITE,
        /*transferable=*/false,
        ChannelEndpoint::destroy,
        ChannelEndpoint::ref
    );
}

Result<HandleId> HandleTable::add(
    const u64 type, void* resource, const capability_set caps,
    const bool transferable,
    void (*destroy)(void*), void (*acquire)(void*)
) {
    SpinlockGuard guard(lock_);

    const int slot = find_free_slot();
    if (slot < 0) return Error::NoMem;

    set_bit(static_cast<usize>(slot));

    HandleEntry& he = entries_[slot];
    he.hid          = type | (static_cast<u64>(slot) & HANDLE_ID_MASK);
    he.type         = type;
    he.resource     = resource;
    he.capabilities = caps;
    he.refcount     = 1;
    he.transferable = transferable;
    he.destroy      = destroy;
    he.acquire      = acquire;

    return Result<HandleId>::ok(he.hid);
}

VoidResult HandleTable::add_at(
    const HandleId fixed_id, const u64 type, void* resource,
    const capability_set caps, const bool transferable,
    void (*destroy)(void*), void (*acquire)(void*)
) {
    const usize slot = fixed_id & HANDLE_ID_MASK;
    if (slot >= MAX_HANDLES_PER_REALM) return Error::BadH;

    SpinlockGuard guard(lock_);
    if (test_bit(slot)) return Error::BadH;

    set_bit(slot);

    HandleEntry& he = entries_[slot];
    he.hid          = fixed_id;
    he.type         = type;
    he.resource     = resource;
    he.capabilities = caps;
    he.refcount     = 1;
    he.transferable = transferable;
    he.destroy      = destroy;
    he.acquire      = acquire;

    return VoidResult::ok();
}

HandleEntry* HandleTable::lookup(const HandleId hid) {
    const usize slot = hid & HANDLE_ID_MASK;
    if (slot >= MAX_HANDLES_PER_REALM) return nullptr;

    SpinlockGuard guard(lock_);
    if (!test_bit(slot)) return nullptr;

    HandleEntry& he = entries_[slot];
    if (he.hid != hid) return nullptr;
    return &he;
}

void HandleTable::acquire(const HandleId hid) {
    if (HandleEntry* he = lookup(hid)) {
        __sync_add_and_fetch(&he->refcount, 1);
    }
}

void HandleTable::release(const HandleId hid) {
    const usize slot = hid & HANDLE_ID_MASK;
    if (slot >= MAX_HANDLES_PER_REALM) return;

    SpinlockGuard guard(lock_);
    if (!test_bit(slot)) return;

    HandleEntry& he = entries_[slot];
    if (he.hid != hid) return;

    if (__sync_sub_and_fetch(&he.refcount, 1) == 0) {
        if (he.destroy && he.resource) he.destroy(he.resource);
        memset(&he, 0, sizeof(HandleEntry));
        clear_bit(slot);
    }
}

void HandleTable::clear() {
    for (usize i = 0; i < MAX_HANDLES_PER_REALM; ++i) {
        if (!test_bit(i)) continue;

        HandleEntry& he = entries_[i];
        if (he.destroy && he.resource) he.destroy(he.resource);
        memset(&he, 0, sizeof(HandleEntry));
        clear_bit(i);
    }
}

bool HandleTable::test_bit(const usize i) const {
    return (bitmap_[i >> 3] >> (i & 7)) & 1;
}

void HandleTable::set_bit(const usize i) {
    bitmap_[i >> 3] |= static_cast<u8>(1u << (i & 7));
}

void HandleTable::clear_bit(const usize i) {
    bitmap_[i >> 3] &= static_cast<u8>(~(1u << (i & 7)));
}

int HandleTable::find_free_slot() const {
    // Slots 0–2 are reserved for stdin/stdout/stderr, slot 3 for vbus.
    for (usize i = 4; i < MAX_HANDLES_PER_REALM; ++i) {
        if (!test_bit(i)) return static_cast<int>(i);
    }
    return -1;
}
