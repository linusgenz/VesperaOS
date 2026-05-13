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

#include <klib/string.h>
#include <vespera/mm/memory.h>
#include <vespera/scheduling.h>
#include <units/unit.h>
#include <vespera_errno.h>

#include <realm/address_space.h>
#include "vespera/realm/realm.h"

static constexpr uptr USER_HEAP_MAX = 0x0000'7FFE'0000'0000ULL;

namespace syscalls::internal {
    i64 sys_brk(u64 addr, u64, u64, u64, u64, u64) {
        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;

        if (cur->heap_start == 0) return -EINVAL;

        const Realm* cur_r = cur->parent;
        PageTableManager* r_ptm = cur_r->address_space->page_table();

        if (addr == 0) return cur->heap_end;
        if (addr < cur->heap_start) return cur->heap_end;
        if (addr > USER_HEAP_MAX)   return -ENOMEM;

        // Grow heap
        if (addr > cur->heap_end) {
            const uptr start = (cur->heap_end + 0xFFF) & ~0xFFFULL;
            const uptr end   = (addr + 0xFFF)          & ~0xFFFULL;

            for (uptr a = start; a < end; a += 0x1000) {
                const phys_addr_t phys = kernel::memory::request_page_phys();
                if (phys_null(phys)) return -ENOMEM;

                memset(phys_to_virt(phys), 0, 0x1000);
                r_ptm->map_memory(
                    virt_from_raw(a), phys,
                    (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
                );
            }

            if (kernel::units::VmArea* vma = cur->find_vma(cur->heap_end, 0)) {
                vma->length = addr - vma->start;
            } else {
                const auto new_vma = new kernel::units::VmArea();
                new_vma->start   = cur->heap_end;
                new_vma->length  = addr - cur->heap_end;
                new_vma->prot    = 0;
                new_vma->flags   = 0;
                new_vma->file_off = 0;
                new_vma->handle  = 0;
                cur->add_vma(new_vma);
            }
        }

        // Shrink heap
        else if (addr < cur->heap_end) {
            const uptr start = (addr + 0xFFF)          & ~0xFFFULL;
            const uptr end   = (cur->heap_end + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += 0x1000) {
                const virt_addr_t vaddr = virt_from_raw(a);
                if (const phys_addr_t phys = r_ptm->get_physical_address(vaddr); !phys_null(phys)) {
                    r_ptm->unmap_memory(vaddr);
                    kernel::memory::free_page_phys(phys);
                }
            }

            if (kernel::units::VmArea* vma = cur->find_vma(addr, 0)) {
                vma->length = addr - vma->start;
                if (vma->length == 0) cur->remove_vma(vma->start, 0);
            }
        }

        cur->heap_end = addr;
        return addr;
    }
}