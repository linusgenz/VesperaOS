// user_stack_allocator.h
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
#ifndef VESPERAOS_USER_STACK_ALLOCATOR_H
#define VESPERAOS_USER_STACK_ALLOCATOR_H

#include <vespera/mm/addr.h>
#include <vespera/types.h>

// Virtual address layout for user stacks within a realm:
//
//  HIGH  0x00007FFF_FF000000  ← USER_STACK_REGION_TOP
//        [ slot N-1 stack  ]  stack_size bytes  (grows down)
//        [ guard page      ]  4 KiB  (unmapped, catches overflow)
//        [ slot N-2 stack  ]
//        [ guard page      ]
//        ...
//        [ slot 0   stack  ]
//  LOW   0x00007FFF_00000000  ← USER_STACK_REGION_BASE

class UserStackAllocator {
   public:
    static constexpr uptr REGION_TOP = 0x00007FFF30000000ULL;
    static constexpr uptr REGION_BASE = 0x00007FFF00000000ULL;

    static constexpr usize GUARD_SIZE = 0x1000;  // 4 KiB

    static constexpr usize MAX_SLOTS = 64;

    void init(usize slot_stack_size);

    struct StackSlot {
        virt_addr_t virt_base;
        virt_addr_t virt_top;
        usize stack_size;
        u32 index;
    };

    bool alloc(StackSlot& out);

    void free(u32 slot_index);

    [[nodiscard]] usize used_count() const;

   private:
    usize slot_stack_size_{0};
    usize slot_size_{0};
    usize max_slots_{0};
    u64 bitmap_{0};       // 1 bit per slot, 1 = in use (up to 64 slots)

    void slot_to_range(u32 index, uptr& base_out, uptr& top_out) const;
};

#endif  // VESPERAOS_USER_STACK_ALLOCATOR_H
