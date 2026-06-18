// dentry_cache.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.06.26.
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

#ifndef VESPERAOS_FILESYSTEM_DENTRY_CACHE_H
#define VESPERAOS_FILESYSTEM_DENTRY_CACHE_H

#include <vespera/types.h>
#include "vespera/sync/spinlock.h"

struct VfsNode;

#ifndef DENTRY_MAX_ENTRIES
#define DENTRY_MAX_ENTRIES 4096
#endif

namespace filesystem {
    constexpr usize DENTRY_MAX_ENTRIES_C = DENTRY_MAX_ENTRIES;
    constexpr usize DENTRY_MAX_NAME = 64;

    struct dentry {
        char name[DENTRY_MAX_NAME];
        VfsNode* node;
        u32 hash;

        // LRU chain
        dentry* lru_prev;
        dentry* lru_next;

        // Tree structure
        dentry* parent;
        dentry* children;
        dentry* sibling_next;

        // Free-list allocation link
        dentry* slab_next;
    };

    class DentryCache {
    public:
        DentryCache() = default;
        ~DentryCache() = default;

        DentryCache(const DentryCache&) = delete;
        DentryCache& operator=(const DentryCache&) = delete;

        void init();

        [[nodiscard]] VfsNode* lookup(const char* path);
        [[nodiscard]] VfsNode* lookup_component(dentry* parent, const char* name, dentry** out_dentry = nullptr);

        dentry* insert_component(dentry* parent, const char* name, VfsNode* node);
        void insert(const char* path, VfsNode* node);

        void invalidate(const char* path);
        void invalidate_prefix(const char* prefix);
        void flush();

        [[nodiscard]] usize entry_count() const;
        [[nodiscard]] usize hit_count() const;
        [[nodiscard]] usize miss_count() const;

    private:
        [[nodiscard]] static u32 hash_name(const char* name);
        [[nodiscard]] dentry* find_child(dentry* parent, const char* name, u32 hash) const;

        void unlink_from_parent(dentry* d);
        void lru_remove(dentry* d);
        void promote(dentry* d);
        dentry* evict_lru();
        usize collect_and_free_subtree(dentry* d, VfsNode** out, usize out_cap);
        void free_slot(dentry* d);

        mutable Spinlock lock_;
        dentry root_{};

        dentry* lru_head_ = nullptr;
        dentry* lru_tail_ = nullptr;

        dentry slab_[DENTRY_MAX_ENTRIES_C] = {};
        dentry* free_list_ = nullptr;

        usize entry_count_ = 0;
        usize hit_count_ = 0;
        usize miss_count_ = 0;
    };

    extern DentryCache g_dentry_cache;
} // namespace filesystem

#endif // VESPERAOS_FILESYSTEM_DENTRY_CACHE_H
