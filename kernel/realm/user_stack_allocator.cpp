// user_stack_allocator.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.04.26.
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

#include <vespera/realm/user_stack_allocator.h>
#include <vespera/log.h>

static constexpr uptr page_align_up(uptr v) {
    return (v + 0xFFFULL) & ~0xFFFULL;
}

void UserStackAllocator::init(usize slot_stack_size) {
    // Ensure the stack size is at least one page and page-aligned.
    slot_stack_size_ = page_align_up(slot_stack_size ? slot_stack_size : 0x1000);

    // Each slot = usable stack + one guard page below it.
    slot_size_ = slot_stack_size_ + GUARD_SIZE;

    const uptr region_size = REGION_TOP - REGION_BASE;
    max_slots_ = region_size / slot_size_;
    if (max_slots_ > MAX_SLOTS) max_slots_ = MAX_SLOTS;

    bitmap_ = 0;
}

void UserStackAllocator::slot_to_range(const u32 index, uptr& base_out, uptr& top_out) const {
    top_out  = REGION_TOP - static_cast<uptr>(index) * slot_size_;
    base_out = top_out - slot_stack_size_;
}

bool UserStackAllocator::alloc(StackSlot& out) {
    if (max_slots_ == 0) {
        Log::warning("UserStackAllocator: not initialized");
        return false;
    }

    for (u32 i = 0; i < static_cast<u32>(max_slots_); ++i) {
        if (!(bitmap_ & (1ULL << i))) {
            bitmap_ |= (1ULL << i);

            uptr base, top;
            slot_to_range(i, base, top);

            out.virt_base  = virt_from_raw(base);
            out.virt_top   = virt_from_raw(top);
            out.stack_size = slot_stack_size_;
            out.index      = i;
            return true;
        }
    }

    Log::warning("UserStackAllocator: all %zu slots exhausted", max_slots_);
    return false;
}

void UserStackAllocator::free(const u32 slot_index) {
    if (slot_index >= max_slots_) {
        Log::warning("UserStackAllocator::free: invalid slot index %u", slot_index);
        return;
    }
    bitmap_ &= ~(1ULL << slot_index);
}

usize UserStackAllocator::used_count() const {
    u64 v = bitmap_;
    usize count = 0;
    while (v) { v &= v - 1; ++count; }
    return count;
}