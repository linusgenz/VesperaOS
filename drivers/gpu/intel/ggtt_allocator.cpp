// ggtt_allocator.cpp
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

#include "ggtt_allocator.h"

#include <vespera/log.h>

namespace blt {

    void GgttAllocator::init(u32 total_entries, u32 start_index) {
        const u32 usable = total_entries - start_index;
        const u32 transient_sz = usable / GGTT_TRANSIENT_FRACTION;
        const u32 persistent_sz = usable - transient_sz;

        persistent_base_ = start_index;
        persistent_next_ = start_index;
        persistent_limit_ = start_index + persistent_sz;

        transient_base_ = persistent_limit_;
        transient_end_ = total_entries;

        // Register the entire transient region as one large free block.
        free_list_[0] = GgttBlock{
            .start_index = transient_base_,
            .num_pages = transient_sz,
            .in_use = false,
        };
        free_list_count_ = 1;

        lock_.init("ggtt_alloc");

        Log::debug(
            "GgttAllocator: persistent=[%u, %u) (%u pages), transient=[%u, %u) (%u pages)",
            persistent_base_,
            persistent_limit_,
            persistent_sz,
            transient_base_,
            transient_end_,
            transient_sz
        );
    }


    u32 GgttAllocator::alloc_persistent(u32 num_pages) {
        SpinlockGuardIrq guard(lock_);

        if (persistent_next_ + num_pages > persistent_limit_) {
            Log::error("GgttAllocator: persistent zone exhausted! (need %u pages)", num_pages);
            return U32_MAX;
        }

        const u32 index = persistent_next_;
        persistent_next_ += num_pages;

        Log::debug("GgttAllocator: persistent alloc %u pages -> index %u", num_pages, index);
        return index;
    }

    u32 GgttAllocator::alloc_transient(u32 num_pages) {
        SpinlockGuardIrq guard(lock_);

        for (usize i = 0; i < free_list_count_; i++) {
            GgttBlock& block = free_list_[i];

            if (block.in_use || block.num_pages < num_pages) {
                continue;
            }

            const u32 alloc_index = block.start_index;

            if (block.num_pages == num_pages) {
                block.in_use = true;
            } else {
                if (free_list_count_ >= GGTT_MAX_FREE_BLOCKS) {
                    Log::error(
                        "GgttAllocator: freelist full, wasting %u pages after index %u",
                        block.num_pages - num_pages,
                        alloc_index + num_pages
                    );
                    block.in_use = true;
                    block.num_pages = num_pages;
                } else {
                    const u32 remainder_start = block.start_index + num_pages;
                    const u32 remainder_pages = block.num_pages - num_pages;

                    block.start_index = alloc_index;
                    block.num_pages = num_pages;
                    block.in_use = true;

                    free_list_[free_list_count_++] = GgttBlock{
                        .start_index = remainder_start,
                        .num_pages = remainder_pages,
                        .in_use = false,
                    };
                }
            }

            Log::debug("GgttAllocator: transient alloc %u pages -> index %u", num_pages, alloc_index);
            return alloc_index;
        }

        Log::error("GgttAllocator: transient zone OOM (need %u pages, %u free)", num_pages, transient_free_pages());
        return U32_MAX;
    }

    void GgttAllocator::free_transient(u32 start_index) {
        SpinlockGuardIrq guard(lock_);

        const int idx = find_block(start_index);

        if (idx < 0) {
            Log::error("GgttAllocator: free_transient called with unknown index %u", start_index);
            return;
        }

        GgttBlock& block = free_list_[idx];

        if (!block.in_use) {
            Log::error("GgttAllocator: double-free detected at GTT index %u!", start_index);
            return;
        }

        block.in_use = false;
        Log::debug("GgttAllocator: freed %u pages at index %u", block.num_pages, start_index);

        // Coalesce after every free to keep the freelist compact and prevent
        // fragmentation under high-frequency small allocations.
        coalesce();
    }

    void GgttAllocator::coalesce() {
        bool merged = true;
        while (merged) {
            merged = false;

            for (usize i = 0; i < free_list_count_ && !merged; i++) {
                if (free_list_[i].in_use) continue;

                for (usize j = 0; j < free_list_count_ && !merged; j++) {
                    if (i == j || free_list_[j].in_use) continue;

                    GgttBlock& a = free_list_[i];
                    GgttBlock& b = free_list_[j];

                    const bool a_before_b = (a.start_index + a.num_pages == b.start_index);
                    const bool b_before_a = (b.start_index + b.num_pages == a.start_index);

                    if (a_before_b || b_before_a) {
                        // The block with the lower start_index absorbs the other.
                        const u32 merged_start = a_before_b ? a.start_index : b.start_index;
                        const u32 merged_pages = a.num_pages + b.num_pages;

                        a.start_index = merged_start;
                        a.num_pages = merged_pages;
                        a.in_use = false;

                        // Remove B by swapping it with the last entry.
                        free_list_[j] = free_list_[--free_list_count_];

                        Log::debug(
                            "GgttAllocator: coalesced -> [%u, %u) (%u pages)",
                            merged_start,
                            merged_start + merged_pages,
                            merged_pages
                        );

                        merged = true;
                    }
                }
            }
        }
    }

    int GgttAllocator::find_block(u32 start_index) const {
        for (usize i = 0; i < free_list_count_; i++) {
            if (free_list_[i].start_index == start_index) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    u32 GgttAllocator::transient_free_pages() const {
        u32 free = 0;
        for (usize i = 0; i < free_list_count_; i++) {
            if (!free_list_[i].in_use) {
                free += free_list_[i].num_pages;
            }
        }
        return free;
    }

}  // namespace blt
