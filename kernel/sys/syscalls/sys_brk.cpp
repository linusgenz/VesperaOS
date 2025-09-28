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
#include <scheduling.h>

static constexpr uintptr_t USER_HEAP_START = 0x40000000;
static constexpr uintptr_t USER_HEAP_MAX   = 0x50000000;

namespace syscalls::internal {
    int64_t sys_brk(uint64_t addr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        // initialize brk if first call
        if (cur->heap_end == 0) {
            cur->heap_end = USER_HEAP_START;
        }

        if (addr == 0) {
            return cur->heap_end;
        }

        if (addr < USER_HEAP_START || addr > USER_HEAP_MAX) {
            return -EINVAL;
        }


        if (addr > cur->heap_end) {
            // grow heap
            uintptr_t current = cur->heap_end;
            size_t npages = (addr - current + 0xFFF) / 0x1000;

            for (size_t i = 0; i < npages; i++) {
                void* page = kernel::memory::request_page();
                if (!page) return -ENOMEM;
                kernel::memory::map_memory((void*)(current + i*0x1000), page, 0x7);
            }
        } else if (addr < cur->heap_end) {
            uintptr_t current = cur->heap_end;
            size_t npages = (current - addr + 0xFFF) / 0x1000;
            for (size_t i = 0; i < npages; i++) {
                uintptr_t vaddr = current - (i+1)*0x1000;
                kernel::memory::unmap_memory((void*)vaddr);
                kernel::memory::free_page((void*)addr);
            }
        }

        cur->heap_end = addr;
        return addr;
    }
}
