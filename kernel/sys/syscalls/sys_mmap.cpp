// sys_mmap.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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
#include <errno.h>
#include <kernel/memory.h>
#include <kernel/scheduling.h>
#include <kernel/sys/mman.h>

namespace syscalls::internal {
    int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot,
                     uint64_t flags, uint64_t handle, uint64_t offset) {
        if (length == 0) return -EINVAL;

        // page align
        addr   = addr & ~(PAGE_SIZE - 1);
        length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        size_t npages = length / PAGE_SIZE;

        // only MAP_ANONYMOUS
        if (!(flags & MAP_ANONYMOUS)) {
            return -EUNSUPPORTED;
        }

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        static uintptr_t next_base = 0x4000000000;
        uintptr_t base = (addr != 0) ? addr : next_base;
        if (addr == 0) {
            next_base += length;
        }
        Log::debug("base %p, base %p\n", base, next_base);

        for (size_t i = 0; i < npages; i++) {
            void* phys = kernel::memory::request_page();
            if (!phys) {
                for (size_t j = 0; j < i; j++) {
                    const auto vaddr = reinterpret_cast<void*>(base + j * PAGE_SIZE);
                    kernel::memory::unmap_memory(vaddr);
                }
                return -ENOMEM;
            }
            const auto vaddr = reinterpret_cast<void*>(base + i * PAGE_SIZE);
            kernel::memory::map_memory(vaddr, phys, (1ULL << PT_Flag::UserSuper));
        }

        auto* area = static_cast<VmArea*>(kernel::memory::malloc(sizeof(VmArea)));
        if (!area) {
            // rollback
            for (size_t i = 0; i < npages; i++) {
                const auto vaddr = reinterpret_cast<void*>(base + i * PAGE_SIZE);
                kernel::memory::unmap_memory(vaddr);
            }
            return -ENOMEM;
        }

        area->start    = base;
        area->length   = length;
        area->prot     = prot;
        area->flags    = flags;
        area->file_off = offset;
        area->handle   = handle;

        cur->add_vma(area);
        Log::debug("ret: %p", base);
        return (int64_t)base;
    }
}
