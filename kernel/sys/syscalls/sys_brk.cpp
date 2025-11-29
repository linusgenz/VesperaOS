// sys_brk.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.09.25.
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

#include <cstdint>
#include <kernel/scheduling.h>

static constexpr uintptr_t USER_HEAP_START = 0x40000000;
static constexpr uintptr_t USER_HEAP_MAX = 0x50000000;

namespace syscalls::internal {
    int64_t sys_brk(uint64_t addr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        Unit *cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        if (cur->heap_end == 0) {
            cur->heap_end = USER_HEAP_START;

            void *page = kernel::memory::request_page();
            if (!page) return -ENOMEM;
            kernel::memory::map_memory((void *) USER_HEAP_START, page, (1ULL << PT_Flag::UserSuper));
        }

        if (addr == 0) return cur->heap_end;

        if (addr < USER_HEAP_START || addr > USER_HEAP_MAX) return -EINVAL;

        if (addr > cur->heap_end) {
            uintptr_t start = (cur->heap_end + 0xFFF) & ~0xFFF;
            uintptr_t end = (addr + 0xFFF) & ~0xFFF;

            for (uintptr_t a = start; a < end; a += 0x1000) {
                void *page = kernel::memory::request_page();
                if (!page) return -ENOMEM;
                kernel::memory::map_memory((void *) a, page, (1ULL << PT_Flag::UserSuper));
            }
        } else if (addr < cur->heap_end) {
            uintptr_t start = (addr + 0xFFF) & ~0xFFF;
            uintptr_t end = (cur->heap_end + 0xFFF) & ~0xFFF;

            for (uintptr_t a = start; a < end; a += 0x1000) {
                void *page = kernel::memory::get_physical_address((void *) a);
                if (page) {
                    kernel::memory::unmap_memory((void *) a);
                    kernel::memory::free_page(page);
                }
            }
        }

        cur->heap_end = addr;
        return addr;
    }
} // namespace syscalls::internal
