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

#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include <vespera/mm/memory.h>
#include "/mnt/ExternerDatentraeger/VesperaOS/kernel/units/unit.h"
#include "vespera/realm/realm.h"
#include "vespera/realm/realm_manager.h"

// TODO
static constexpr uptr USER_HEAP_START = 0x40000000;
static constexpr uptr USER_HEAP_MAX = 0x50000000;

namespace syscalls::internal {
    i64 sys_brk(u64 addr, u64, u64, u64, u64, u64) {
        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;
        const Realm* cur_r = RealmManager::get(cur->rid);

        // Initialize heap on first call
        if (cur->heap_end == 0) {
            cur->heap_end = USER_HEAP_START;

            phys_addr_t phys = kernel::memory::request_pages_phys(0x20000 / PAGE_SIZE);
            if (phys_null(phys)) return -ENOMEM;

            memset(phys_to_virt(phys), 0, 0x20000);

            cur_r->page_table->map_range(
                virt_from_raw(USER_HEAP_START),
                phys,
                0x20000,
                (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
            );

            const auto vma = new VmArea();
            vma->start = USER_HEAP_START;
            vma->length = 0x20000;
            vma->prot = 0;
            vma->flags = 0;
            vma->file_off = 0;
            vma->handle = 0;
            cur->add_vma(vma);
        }

        if (addr == 0) return cur->heap_end;

        if (addr < USER_HEAP_START || addr > USER_HEAP_MAX) return -EINVAL;

        // Grow heap
        if (addr > cur->heap_end) {
            uptr start = (cur->heap_end + 0xFFF) & ~0xFFFULL;
            uptr end = (addr + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += 0x1000) {
                phys_addr_t phys = kernel::memory::request_page_phys();
                if (phys_null(phys)) return -ENOMEM;

                memset(phys_to_virt(phys), 0, 0x1000);

                cur_r->page_table->map_memory(
                    virt_from_raw(a),
                    phys,
                    (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
                );
            }

            if (VmArea* vma = cur->find_vma(cur->heap_end, 0)) {
                vma->length = addr - vma->start;
            } else {
                const auto new_vma = new VmArea();
                new_vma->start = cur->heap_end;
                new_vma->length = addr - cur->heap_end;
                new_vma->prot = 0;
                new_vma->flags = 0;
                new_vma->file_off = 0;
                new_vma->handle = 0;
                cur->add_vma(new_vma);
            }
        }

        // Shrink heap
        else if (addr < cur->heap_end) {
            uptr start = (addr + 0xFFF) & ~0xFFFULL;
            uptr end = (cur->heap_end + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += 0x1000) {
                const virt_addr_t vaddr = virt_from_raw(a);
                if (const phys_addr_t phys = cur_r->page_table->get_physical_address(vaddr); !phys_null(phys)) {
                    cur_r->page_table->unmap_memory(vaddr);
                    kernel::memory::free_page_phys(phys);
                }
            }

            if (VmArea* vma = cur->find_vma(addr, 0)) {
                vma->length = addr - vma->start;
                if (vma->length == 0) cur->remove_vma(vma->start, vma->length);
            }
        }

        cur->heap_end = addr;
        return addr;
    }
}  // namespace syscalls::internal
