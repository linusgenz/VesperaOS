// file_backing.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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

#include <klib/string.h>
#include <vespera/mm/memory.h>
#include <vespera/mm/file_backing.h>

#include <filesystem/vfs.h>
#include "klib/utils.h"

namespace {
    // Maximum number of distinct files that can have an active shared
    // FileBackingObject at once.
    constexpr usize FILE_BACKING_TABLE_SIZE = 256;

    struct FileBackingTableEntry {
        const MountPoint* mount = nullptr;  // nullptr => slot free
        u64 inode_id = 0;
        FileBackingObject* obj = nullptr;
    };

    FileBackingTableEntry g_file_backing_table[FILE_BACKING_TABLE_SIZE];

    Spinlock g_file_backing_table_lock{"file_backing_table_lock"};
}  // namespace

FileBackingObject* FileBackingObject::get_or_create(VfsNode* node) {
    if (!node) return nullptr;

    // Nodes whose driver hasn't populated inode_id can't be safely shared
    // across separate open() calls (0 isn't a reliable per-file identity), so
    // each such mapping gets its own private, unshared backing object.
    if (node->inode_id == 0) {
        return new FileBackingObject(node);
    }

    SpinlockGuard sg(g_file_backing_table_lock);

    int free_slot = -1;
    for (usize i = 0; i < FILE_BACKING_TABLE_SIZE; i++) {
        FileBackingTableEntry& e = g_file_backing_table[i];
        if (e.mount == node->mount && e.inode_id == node->inode_id && e.obj != nullptr) {
            return e.obj;
        }
        if (free_slot < 0 && e.obj == nullptr) {
            free_slot = static_cast<int>(i);
        }
    }

    if (free_slot < 0) return nullptr;  // table full

    auto* obj = new FileBackingObject(node);
    if (!obj) return nullptr;

    g_file_backing_table[free_slot] = {node->mount, node->inode_id, obj};
    return obj;
}

FileBackingObject::FileBackingObject(VfsNode* node)
    : node_(ref_node(node))
    , size_(node ? node->size : 0)
    , mount_(node ? node->mount : nullptr)
    , inode_id_(node ? node->inode_id : 0) {
    lock_.init("file_backing_lock");

    if (size_ > 0) {
        page_count_ = (size_ + PAGE_SIZE - 1) / PAGE_SIZE;
        pages_ = static_cast<phys_addr_t*>(
            kernel::memory::malloc(page_count_ * sizeof(phys_addr_t))
        );
        if (pages_) {
            memset(pages_, 0, page_count_ * sizeof(phys_addr_t));
        }
    }
}

FileBackingObject::~FileBackingObject() {
    if (pages_) {
        for (usize i = 0; i < page_count_; i++) {
            if (!phys_null(pages_[i])) {
                kernel::memory::free_page_phys(pages_[i]);
            }
        }
        kernel::memory::free(pages_);
    }

    if (node_) {
        unref_node(node_);
    }
}

phys_addr_t FileBackingObject::get_page(usize offset_in_bytes) {
    SpinlockGuard sg(lock_);

    usize page_idx = offset_in_bytes / PAGE_SIZE;
    if (page_idx >= page_count_ || !pages_) {
        return make_phys(0);
    }

    phys_addr_t phys = pages_[page_idx];

    if (phys_null(phys)) {
        phys = kernel::memory::request_page_phys();
        if (phys_null(phys)) {
            return make_phys(0);
        }

        void* virt = virt_ptr(phys_to_virt(phys));
        memset(virt, 0, PAGE_SIZE);

        if (node_ && offset_in_bytes < size_) {
            const usize read_bytes = min(PAGE_SIZE, size_ - offset_in_bytes);

            lock_.unlock();
            VFS::read(node_, offset_in_bytes, read_bytes, virt);
            lock_.lock();
        }

        pages_[page_idx] = phys;
    }

    return phys;
}

void FileBackingObject::sync_page(usize offset_in_bytes, phys_addr_t phys, bool is_dirty) {
    if (!is_dirty || phys_null(phys) || !node_) return;

    SpinlockGuard sg(lock_);

    if (offset_in_bytes < size_) {
        void* virt = virt_ptr(phys_to_virt(phys));
        usize write_bytes = min(PAGE_SIZE, size_ - offset_in_bytes);

        lock_.unlock();
        VFS::write(node_, offset_in_bytes, write_bytes, virt);
        lock_.lock();
    }
}

void FileBackingObject::add_mapping() {
    SpinlockGuard sg(lock_);
    mapping_count_++;
}

void FileBackingObject::remove_mapping() {
    {
        SpinlockGuard sg(lock_);
        if (mapping_count_ > 0) mapping_count_--;
    }
    check_and_destroy();
}

usize FileBackingObject::get_size() const {
    SpinlockGuard sg(lock_);
    return size_;
}

void FileBackingObject::check_and_destroy() const {
    bool destroy = false;
    {
        SpinlockGuard sg(lock_);
        if (mapping_count_ == 0) {
            destroy = true;
        }
    }

    if (destroy) {
        if (inode_id_ != 0) {
            SpinlockGuard sg(g_file_backing_table_lock);
            for (auto& e : g_file_backing_table) {
                if (e.obj == this) {
                    e = {};
                    break;
                }
            }
        }
        delete this;
    }
}