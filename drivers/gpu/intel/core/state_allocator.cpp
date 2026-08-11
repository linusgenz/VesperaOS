// state_allocator.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.08.26.
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

#include "state_allocator.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

namespace gpu::intel::core {

    namespace {

        [[nodiscard]] constexpr u32 align_up(const u32 value, const u32 align) {
            // align must be a power of two.
            return (value + align - 1) & ~(align - 1);
        }

    }  // namespace

    bool StateAllocator::init(
        GgttAllocator& ggtt, const u32 persistent_bytes, const u32 frame_bytes, const u32 frame_count,
        const u8 pat_index
    ) {
        if (frame_count == 0 || frame_count > MAX_FRAME_SLOTS) {
            Log::error(
                "state-alloc: frame_count=%u out of range (1..%u)", frame_count, MAX_FRAME_SLOTS
            );
            return false;
        }

        // 64B: the coarsest alignment any of the sub-allocations we hand out
        // needs (SF_CLIP_VIEWPORT, RENDER_SURFACE_STATE, ...). Rounding the
        // zone sizes up to it keeps the transient slots and the zone
        // boundary itself always aligned, so callers never have to think
        // about crossing a slot edge with a mis-aligned start.
        constexpr u32 ZONE_ALIGN = 64;

        persistent_capacity_ = align_up(persistent_bytes, ZONE_ALIGN);
        frame_capacity_ = align_up(frame_bytes, ZONE_ALIGN);
        frame_count_ = frame_count;

        const u32 total_bytes = persistent_capacity_ + frame_capacity_ * frame_count_;
        const u32 pages = (total_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

        const GgttAllocation alloc = ggtt.alloc_persistent(pages, (1ULL << CacheDisabled), pat_index);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("state-alloc: GGTT allocation failed (%u pages)", pages);
            return false;
        }

        cpu_base_ = alloc.cpu_addr;
        gfx_base_ = alloc.gfx_addr;
        pages_ = pages;

        memset(virt_ptr(cpu_base_), 0, static_cast<usize>(pages) * PAGE_SIZE);

        persistent_next_ = 0;

        frame_slot_ = 0;
        frame_cursor_ = 0;
        frame_started_ = false;
        for (u32 i = 0; i < MAX_FRAME_SLOTS; i++) {
            slot_fence_[i] = 0;
        }

        Log::info(
            "state-alloc: ready GFX=0x%llx pages=%u persistent=%u B frame=%u B x%u slots (total %u B)",
            base_gfx_addr(), pages_, persistent_capacity_, frame_capacity_, frame_count_, total_bytes
        );

        return true;
    }

    StateAllocator::StateAlloc StateAllocator::alloc_persistent(const u32 size, const u32 align) {
        const u32 aligned_start = align_up(persistent_next_, align);

        if (aligned_start + size > persistent_capacity_) {
            Log::log_dbc(
                "state-alloc: persistent zone exhausted (need %u, %u free of %u)",
                size, persistent_capacity_ - persistent_next_, persistent_capacity_
            );
            return {};
        }

        persistent_next_ = aligned_start + size;

        return StateAlloc{
            .offset = aligned_start,
            .cpu_ptr = static_cast<u8*>(base_cpu_addr()) + aligned_start,
        };
    }

    u64 StateAllocator::begin_frame() {
        // First call ever: land on slot 0 without treating it as "advance
        // from slot 0", so slot 0's fence (legitimately 0 / never-used) is
        // reported instead of skipping straight to slot 1.
        if (!frame_started_) {
            frame_started_ = true;
        } else {
            frame_slot_ = (frame_slot_ + 1) % frame_count_;
        }

        frame_cursor_ = 0;
        return slot_fence_[frame_slot_];
    }

    StateAllocator::StateAlloc StateAllocator::alloc_transient(const u32 size, const u32 align) {
        const u32 aligned_start = align_up(frame_cursor_, align);

        if (aligned_start + size > frame_capacity_) {
            Log::log_dbc(
                "state-alloc: frame slot %u exhausted (need %u, %u free of %u)",
                frame_slot_, size, frame_capacity_ - frame_cursor_, frame_capacity_
            );
            return {};
        }

        frame_cursor_ = aligned_start + size;

        const u32 slot_base = persistent_capacity_ + frame_slot_ * frame_capacity_;
        const u32 offset = slot_base + aligned_start;

        return StateAlloc{
            .offset = offset,
            .cpu_ptr = static_cast<u8*>(base_cpu_addr()) + offset,
        };
    }

    void StateAllocator::end_frame(const u64 seqno) {
        slot_fence_[frame_slot_] = seqno;
    }

    void StateAllocator::flush_range(const void* ptr, const usize size) {
        constexpr usize CACHE_LINE = 64;
        const auto* p = static_cast<const u8*>(ptr);
        const usize end = reinterpret_cast<usize>(p) + size;

        for (usize line = reinterpret_cast<usize>(p) & ~(CACHE_LINE - 1); line < end; line += CACHE_LINE) {
            asm volatile("clflush (%0)" ::"r"(line) : "memory");
        }
        asm volatile("mfence" ::: "memory");
    }

}  // namespace gpu::intel::core
