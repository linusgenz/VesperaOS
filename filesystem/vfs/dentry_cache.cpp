// dentry_cache.cpp
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

#include "dentry_cache.h"

#include <vespera/types.h>

#include "filesystem/vfs.h"
#include "filesystem/vfs_node.h"
#include "klib/string.h"

namespace filesystem {
    static void cache_unref(VfsNode* node) {
        if (node)
            VFS::close(node);
    }

    DentryCache g_dentry_cache;

    u32 DentryCache::hash_name(const char* name) {
        // FNV-1a 32-bit
        constexpr u32 FNV_OFFSET_BASIS = 0x811c9dc5u;
        constexpr u32 FNV_PRIME = 0x01000193u;

        u32 h = FNV_OFFSET_BASIS;
        for (const char* p = name; *p != '\0'; ++p) {
            h ^= static_cast<u32>(static_cast<u8>(*p));
            h *= FNV_PRIME;
        }
        return h;
    }

    void DentryCache::init() {
        lock_.init("dentry_cache_lock");

        // Root dentry is static and never goes through the slab/free-list.
        root_.name[0] = '/';
        root_.name[1] = '\0';
        root_.node = nullptr;
        root_.hash = hash_name(root_.name);
        root_.lru_prev = nullptr;
        root_.lru_next = nullptr;
        root_.parent = nullptr;
        root_.children = nullptr;
        root_.sibling_next = nullptr;
        root_.slab_next = nullptr;

        // Build the free-list by chaining all slab entries via slab_next.
        for (usize i = 0; i < DENTRY_MAX_ENTRIES_C; ++i) {
            slab_[i].slab_next = (i + 1 < DENTRY_MAX_ENTRIES_C) ? &slab_[i + 1] : nullptr;
            slab_[i].node = nullptr;
            slab_[i].lru_prev = nullptr;
            slab_[i].lru_next = nullptr;
            slab_[i].parent = nullptr;
            slab_[i].children = nullptr;
            slab_[i].sibling_next = nullptr;
        }
        free_list_ = &slab_[0];
        lru_head_ = nullptr;
        lru_tail_ = nullptr;
        entry_count_ = 0;
        hit_count_ = 0;
        miss_count_ = 0;
    }

    dentry* DentryCache::find_child(dentry* parent, const char* name, const u32 hash) const {
        dentry* head = parent ? parent->children : root_.children;
        dentry* d = head;
        while (d) {
            if (d->hash == hash && strcmp(d->name, name) == 0)
                return d;
            d = d->sibling_next;
        }
        return nullptr;
    }

    void DentryCache::unlink_from_parent(dentry* d) {
        dentry* parent = d->parent;
        dentry** pp = parent ? &parent->children : &root_.children;
        while (*pp && *pp != d)
            pp = &(*pp)->sibling_next;
        if (*pp)
            *pp = d->sibling_next;
        d->sibling_next = nullptr;
    }

    void DentryCache::lru_remove(dentry* d) {
        if (d->lru_prev)
            d->lru_prev->lru_next = d->lru_next;
        else
            lru_head_ = d->lru_next;

        if (d->lru_next)
            d->lru_next->lru_prev = d->lru_prev;
        else
            lru_tail_ = d->lru_prev;

        d->lru_prev = nullptr;
        d->lru_next = nullptr;
    }

    void DentryCache::promote(dentry* d) {
        // Root is never on the LRU list.
        if (d == &root_)
            return;

        // Already at MRU end — nothing to do.
        if (d == lru_tail_)
            return;

        lru_remove(d);

        // Append at tail (MRU end).
        d->lru_prev = lru_tail_;
        d->lru_next = nullptr;
        if (lru_tail_)
            lru_tail_->lru_next = d;
        else
            lru_head_ = d; // List was empty.
        lru_tail_ = d;
    }

    usize DentryCache::collect_and_free_subtree(dentry* d, VfsNode** out, const usize out_cap) {
        usize n = 0;

        // Recursively collect children first.
        dentry* child = d->children;
        while (child) {
            dentry* next = child->sibling_next; // save before child is freed
            if (n < out_cap)
                n += collect_and_free_subtree(child, out + n, out_cap - n);
            child = next;
        }
        d->children = nullptr;

        // Now detach and free d itself.
        lru_remove(d);
        if (n < out_cap) {
            out[n] = d->node;
            ++n;
        }
        d->node = nullptr;
        --entry_count_;
        free_slot(d);

        return n;
    }

    void DentryCache::free_slot(dentry* d) {
        d->slab_next = free_list_;
        free_list_ = d;
    }

    dentry* DentryCache::evict_lru() {
        dentry* victim = lru_head_;
        if (!victim)
            return nullptr;

        // Detach victim from its parent's children list up front; its
        // own subtree (if any) is unlinked from victim by
        // collect_and_free_subtree itself.
        unlink_from_parent(victim);

        VfsNode* evicted_nodes[DENTRY_MAX_ENTRIES_C];
        const usize count = collect_and_free_subtree(victim, evicted_nodes, DENTRY_MAX_ENTRIES_C);

        // Release the spinlock while calling back into VFS to avoid a deadlock
        // if ops->close() triggers another VFS operation.
        lock_.unlock();
        for (usize i = 0; i < count; ++i)
            cache_unref(evicted_nodes[i]);
        lock_.lock();

        // free_slot() pushed `victim` (and any evicted descendants) onto
        // free_list_ already; hand back the slot for the original victim
        // request specifically, since it's guaranteed freed.
        dentry* slot = free_list_;
        free_list_ = slot->slab_next;
        slot->slab_next = nullptr;
        return slot;
    }

    VfsNode* DentryCache::lookup_component(dentry* parent, const char* name, dentry** out_dentry) {
        if (!name || !*name)
            return nullptr;

        const u32 h = hash_name(name);

        SpinlockGuard g(lock_);

        dentry* d = find_child(parent, name, h);
        if (!d) {
            ++miss_count_;
            if (out_dentry)
                *out_dentry = nullptr;
            return nullptr;
        }

        ++hit_count_;
        promote(d);
        __atomic_fetch_add(&d->node->ref_count, 1, __ATOMIC_RELAXED);
        if (out_dentry)
            *out_dentry = d;
        return d->node;
    }

    VfsNode* DentryCache::lookup(const char* path) {
        if (!path || !*path)
            return nullptr;

        // Root path resolves directly; root has no cached VfsNode of its
        // own (there is no insert target for "/"), so this is always a miss.
        if (path[0] == '/' && path[1] == '\0')
            return nullptr;

        dentry* parent = nullptr; // nullptr == root, matches find_child()'s convention
        VfsNode* result = nullptr;
        VfsNode* prev_result = nullptr;

        const char* p = path;
        if (*p == '/')
            ++p;

        char component[DENTRY_MAX_NAME];

        while (*p) {
            usize len = 0;
            while (p[len] && p[len] != '/')
                ++len;

            if (len == 0 || len >= DENTRY_MAX_NAME) {
                // Empty component (e.g. "//") or component too long to have
                // ever been cached: treat the whole lookup as a miss.
                SpinlockGuard g(lock_);
                ++miss_count_;
                return nullptr;
            }

            memcpy(component, p, len);
            component[len] = '\0';

            dentry* matched = nullptr;
            prev_result = result;
            result = lookup_component(parent, component, &matched);

            if (prev_result && prev_result != result) {
                __atomic_fetch_sub(&prev_result->ref_count, 1, __ATOMIC_RELAXED);
            }

            if (!matched)
                return nullptr; // lookup_component() already bumped miss_count_.

            parent = matched;
            p += len;
            if (*p == '/')
                ++p;
        }

        return result;
    }

    dentry* DentryCache::insert_component(dentry* parent, const char* name, VfsNode* node) {
        if (!name || !*name || !node)
            return nullptr;

        const usize len = strlen(name);
        if (len >= DENTRY_MAX_NAME)
            return nullptr;

        const u32 h = hash_name(name);

        VfsNode* old_node = nullptr;
        dentry* result;

        {
            SpinlockGuard g(lock_);

            dentry* existing = find_child(parent, name, h);
            if (existing) {
                old_node = existing->node;
                existing->node = node;
                __atomic_fetch_add(&node->ref_count, 1, __ATOMIC_RELAXED);
                promote(existing);
                result = existing;
            } else {
                dentry* d = nullptr;
                if (free_list_) {
                    d = free_list_;
                    free_list_ = d->slab_next;
                    d->slab_next = nullptr;
                } else {
                    d = evict_lru();
                    if (!d)
                        return nullptr;
                }

                strncpy(d->name, name, DENTRY_MAX_NAME - 1);
                d->name[DENTRY_MAX_NAME - 1] = '\0';
                d->node = node;
                d->hash = h;
                d->parent = parent;
                d->children = nullptr;
                __atomic_fetch_add(&node->ref_count, 1, __ATOMIC_RELAXED);

                dentry** head = parent ? &parent->children : &root_.children;
                d->sibling_next = *head;
                *head = d;

                d->lru_prev = lru_tail_;
                d->lru_next = nullptr;
                if (lru_tail_)
                    lru_tail_->lru_next = d;
                else
                    lru_head_ = d;
                lru_tail_ = d;

                ++entry_count_;
                result = d;
            }
        }

        if (old_node)
            cache_unref(old_node);

        return result;
    }

    void DentryCache::insert(const char* path, VfsNode* node) {
        if (!path || !*path || !node)
            return;

        // Root path itself is never a cache target; nothing to insert.
        if (path[0] == '/' && path[1] == '\0')
            return;

        dentry* parent = nullptr; // nullptr == root
        const char* p = path;
        if (*p == '/')
            ++p;

        char component[DENTRY_MAX_NAME];

        while (*p) {
            usize len = 0;
            while (p[len] && p[len] != '/')
                ++len;

            if (len == 0 || len >= DENTRY_MAX_NAME)
                return; // Malformed or too-long component: give up silently, as before.

            memcpy(component, p, len);
            component[len] = '\0';

            p += len;
            const bool is_last = (*p == '\0');
            if (*p == '/')
                ++p;

            // Only the final component maps to `node`; intermediate
            // components are best inserted by the caller as it walks
            // (via insert_component()), since this cache has no way to
            // resolve intermediate VfsNode*s on its own. If an
            // intermediate component is not yet cached, we cannot
            // synthesize one here without its VfsNode, so we stop.
            if (is_last) {
                insert_component(parent, component, node);
                return;
            }

            dentry* matched = nullptr;
            lookup_component(parent, component, &matched);
            if (!matched)
                return; // Intermediate not cached: caller should use insert_component() while walking instead.
            parent = matched;
        }
    }

    void DentryCache::invalidate(const char* path) {
        if (!path || !*path)
            return;

        // Path was "/": nothing to invalidate (root has no VfsNode entry).
        if (path[0] == '/' && path[1] == '\0')
            return;

        dentry* parent = nullptr;
        dentry* target = nullptr;

        const char* p = path;
        if (*p == '/')
            ++p;

        char component[DENTRY_MAX_NAME];

        SpinlockGuard g(lock_);

        while (*p) {
            usize len = 0;
            while (p[len] && p[len] != '/')
                ++len;

            if (len == 0 || len >= DENTRY_MAX_NAME)
                return;

            memcpy(component, p, len);
            component[len] = '\0';

            const u32 h = hash_name(component);
            dentry* d = find_child(parent, component, h);
            if (!d)
                return; // Not cached: nothing to invalidate.

            p += len;
            if (*p == '/')
                ++p;

            parent = d;
            target = d;
        }

        if (!target)
            return;

        unlink_from_parent(target);

        VfsNode* evicted_nodes[DENTRY_MAX_ENTRIES_C];
        const usize count = collect_and_free_subtree(target, evicted_nodes, DENTRY_MAX_ENTRIES_C);

        // Release the lock while calling back into VFS, matching the
        // deadlock-avoidance pattern used elsewhere in this file. The
        // SpinlockGuard `g` re-acquires the lock on scope exit via its
        // destructor expecting to unlock once, so we re-lock here to keep
        // that invariant intact.
        lock_.unlock();
        for (usize i = 0; i < count; ++i)
            cache_unref(evicted_nodes[i]);
        lock_.lock();
    }

    void DentryCache::invalidate_prefix(const char* prefix) {
        // In the tree model, invalidating a node already drops its whole
        // subtree, so invalidating by prefix is just invalidating the
        // dentry that exactly matches `prefix`.
        invalidate(prefix);
    }

    void DentryCache::flush() {
        VfsNode* to_unref[DENTRY_MAX_ENTRIES_C];
        usize unref_count = 0;

        {
            SpinlockGuard g(lock_);

            for (auto & i : slab_) {
                if (i.node)
                    to_unref[unref_count++] = i.node;
            }

            root_.children = nullptr;

            for (usize i = 0; i < DENTRY_MAX_ENTRIES_C; ++i) {
                slab_[i].node = nullptr;
                slab_[i].lru_prev = nullptr;
                slab_[i].lru_next = nullptr;
                slab_[i].parent = nullptr;
                slab_[i].children = nullptr;
                slab_[i].sibling_next = nullptr;
                slab_[i].slab_next = (i + 1 < DENTRY_MAX_ENTRIES_C) ? &slab_[i + 1] : nullptr;
            }
            free_list_ = &slab_[0];
            lru_head_ = nullptr;
            lru_tail_ = nullptr;
            entry_count_ = 0;
        } // lock released here

        for (usize i = 0; i < unref_count; ++i)
            cache_unref(to_unref[i]);
    }

    usize DentryCache::entry_count() const {
        SpinlockGuard g(lock_);
        return entry_count_;
    }

    usize DentryCache::hit_count() const {
        SpinlockGuard g(lock_);
        return hit_count_;
    }

    usize DentryCache::miss_count() const {
        SpinlockGuard g(lock_);
        return miss_count_;
    }
} // namespace filesystem