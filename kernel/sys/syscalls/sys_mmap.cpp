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

#include <vespera/scheduling.h>
#include <vespera/sys/mman.h>
#include <vespera_errno.h>

#include <vespera/log.h>
#include <vespera/mm/memory.h>

// TODO FIX MMAP. USE PROT AND USE MAP_*
namespace syscalls::internal {
    int64_t sys_mmap(uint64_t addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t handle, uint64_t offset) {
        if (length == 0) return -EINVAL;

        addr   = addr & ~(PAGE_SIZE - 1);
        length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        size_t npages = length / PAGE_SIZE;

        if (!(flags & MAP_ANONYMOUS)) return -EUNSUPPORTED;

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        static uintptr_t next_base = 0x4000000000;
        uintptr_t base = (addr != 0) ? addr : next_base;
        if (addr == 0) next_base += length;

        Log::debug("base %p, next %p\n", base, next_base);

        for (size_t i = 0; i < npages; i++) {
            phys_addr_t phys = kernel::memory::request_page_phys();
            if (phys_null(phys)) {
                for (size_t j = 0; j < i; j++)
                    kernel::memory::unmap_memory(virt_from_raw(base + j * PAGE_SIZE));
                return -ENOMEM;
            }
            kernel::memory::map_memory(virt_from_raw(base + i * PAGE_SIZE), phys, (1ULL << UserSuper));
        }

        auto* area = static_cast<VmArea*>(kernel::memory::malloc(sizeof(VmArea)));
        if (!area) {
            for (size_t i = 0; i < npages; i++)
                kernel::memory::unmap_memory(virt_from_raw(base + i * PAGE_SIZE));
            return -ENOMEM;
        }

        area->start   = base;
        area->length  = length;
        area->prot    = prot;
        area->flags   = flags;
        area->file_off = offset;
        area->handle  = handle;

        cur->add_vma(area);
        Log::debug("ret: %p", base);
        return static_cast<int64_t>(base);
    }
}  // namespace syscalls::internal
