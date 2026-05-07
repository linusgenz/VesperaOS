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

#include <vespera/mm/memory.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/sys/mman.h>
#include <vespera_errno.h>

#include "../../realm/address_space.h"

static uptr find_free_range(const Unit* u, const usize length) {
    static constexpr uptr MMAP_BASE = 0x0000600000000000ULL;
    static constexpr uptr MMAP_END  = 0x00007FFFFFF00000ULL;

    uptr current = MMAP_BASE;

    while (true) {
        const VmArea* next = nullptr;
        uptr next_start = MMAP_END;

        for (const VmArea* v = u->get_vma_list(); v; v = v->next) {
            if (v->start >= current && v->start < next_start) {
                next = v;
                next_start = v->start;
            }
        }

        if (!next) {
            if (current + length <= MMAP_END)
                return current;
            return 0;
        }

        if (current + length <= next->start) {
            return current;
        }

        const uptr end = next->start + next->length;
        current = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        if (current >= MMAP_END)
            return 0;
    }
}

// TODO FIX MMAP. USE PROT AND USE MAP_*
namespace syscalls::internal {
    i64 sys_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 handle, u64 offset) {
        if (length == 0) return -EINVAL;

        addr   = addr & ~(PAGE_SIZE - 1);
        length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const usize npages = length / PAGE_SIZE;

        if (!(flags & MAP_ANONYMOUS)) return -EUNSUPPORTED;

        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        const Realm* cur_r = cur->parent;
        PageTableManager* r_ptm = cur_r->address_space->page_table();

        uptr base = 0;

        if (addr != 0) {
            // TODO, do bound/security checks etc.
            base = addr;
        } else {
            base = find_free_range(cur, length);
            if (base == 0) return -ENOMEM;
        }

        for (usize i = 0; i < npages; i++) {
            const phys_addr_t phys = kernel::memory::request_page_phys();
            if (phys_null(phys)) {
                for (usize j = 0; j < i; j++)
                     r_ptm->unmap_memory(virt_from_raw(base + j * PAGE_SIZE));
                return -ENOMEM;
            }
            r_ptm->map_memory(virt_from_raw(base + i * PAGE_SIZE), phys, (1ULL << UserSuper)| (1ULL << ReadWrite));
        }

        auto* area = static_cast<VmArea*>(kernel::memory::malloc(sizeof(VmArea)));
        if (!area) {
            for (usize i = 0; i < npages; i++)
                 r_ptm->unmap_memory(virt_from_raw(base + i * PAGE_SIZE));
            return -ENOMEM;
        }

        area->start   = base;
        area->length  = length;
        area->prot    = prot;
        area->flags   = flags;
        area->file_off = offset;
        area->handle  = handle;

        cur->add_vma(area);
        return static_cast<i64>(base);
    }
}  // namespace syscalls::internal
