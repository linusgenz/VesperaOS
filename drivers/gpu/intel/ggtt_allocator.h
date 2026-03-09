// ggtt_allocator.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.03.26.
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
#ifndef VESPERAOS_GGTT_ALLOCATOR_H
#define VESPERAOS_GGTT_ALLOCATOR_H

#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

namespace blt {

    static constexpr usize GGTT_MAX_FREE_BLOCKS = 64;

    // Fraction of the usable GGTT reserved for transient allocations (1/4).
    static constexpr usize GGTT_TRANSIENT_FRACTION = 4;

    struct GgttBlock {
        u32 start_index;  // GTT page index of the first page in this block
        u32 num_pages;    // Size of the block in pages
        bool in_use;      // true = allocated, false = free
    };

    class GgttAllocator {
       public:
        GgttAllocator() = default;

        void init(u32 total_entries, u32 start_index);

        [[nodiscard]] u32 alloc_persistent(u32 num_pages);

        [[nodiscard]] u32 alloc_transient(u32 num_pages);

        void free_transient(u32 start_index);

        // ── Diagnostics ──────────────────────────────────────────────────────────
        u32 persistent_used_pages() const {
            return persistent_next_ - persistent_base_;
        }
        u32 transient_total_pages() const {
            return transient_end_ - transient_base_;
        }
        u32 transient_free_pages() const;
        u32 transient_used_pages() const {
            return transient_total_pages() - transient_free_pages();
        }

       private:
        void coalesce();

        int find_block(u32 start_index) const;

        u32 persistent_base_ = 0;
        u32 persistent_next_ = 0;   // next free page index (bump pointer)
        u32 persistent_limit_ = 0;  // exclusive upper bound of the persistent zone

        u32 transient_base_ = 0;
        u32 transient_end_ = 0;

        GgttBlock free_list_[GGTT_MAX_FREE_BLOCKS] = {};
        usize free_list_count_ = 0;

        mutable Spinlock lock_;
    };

}  // namespace blt

#endif  // VESPERAOS_GGTT_ALLOCATOR_H
