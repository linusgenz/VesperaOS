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

#include <vespera_errno.h>
#include <kernel/scheduling.h>

#include "/mnt/ExternerDatentraeger/VesperaOS/kernel/units/unit.h"
#include "kernel/memory.h"
#include "kernel/realm/realm.h"
#include "kernel/realm/realm_manager.h"

// TODO
static constexpr uintptr_t USER_HEAP_START = 0x40000000;
static constexpr uintptr_t USER_HEAP_MAX = 0x50000000;

namespace syscalls::internal {
    int64_t sys_brk(uint64_t addr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        Unit* cur = kernel::scheduling::get_current_unit();
        if (!cur || !cur->is_user) return -EACCES;
        const Realm* cur_r = RealmManager::get(cur->rid);

        // Initialize heap on first call
        if (cur->heap_end == 0) {
            cur->heap_end = USER_HEAP_START;

            uint64_t phys = kernel::memory::request_pages_phys(0x20000 / PAGE_SIZE);
            if (!phys) return -ENOMEM;

            // Kernel kann via HHDM nullen
            memset(phys_to_virt(phys), 0, 0x20000);

            cur_r->page_table->map_range(
                reinterpret_cast<void*>(USER_HEAP_START),
                reinterpret_cast<void*>(phys),
                0x20000,
                (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite) | (1ULL << PT_Flag::UserSuper)
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
            uintptr_t start = (cur->heap_end + 0xFFF) & ~0xFFFULL;
            uintptr_t end = (addr + 0xFFF) & ~0xFFFULL;

            for (uintptr_t a = start; a < end; a += 0x1000) {
                uint64_t phys = kernel::memory::request_page_phys();
                if (!phys) return -ENOMEM;

                memset(phys_to_virt(phys), 0, 0x1000);

                cur_r->page_table->map_memory(
                    reinterpret_cast<void*>(a),
                    reinterpret_cast<void*>(phys),
                    (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite) | (1ULL << PT_Flag::UserSuper)
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
            uintptr_t start = (addr + 0xFFF) & ~0xFFFULL;
            uintptr_t end = (cur->heap_end + 0xFFF) & ~0xFFFULL;

            for (uintptr_t a = start; a < end; a += 0x1000) {
                if (void* phys = cur_r->page_table->get_physical_address(reinterpret_cast<void*>(a))) {
                    cur_r->page_table->unmap_memory(reinterpret_cast<void*>(a));
                    kernel::memory::free_page_phys(reinterpret_cast<uint64_t>(phys));
                }
            }

            if (VmArea* vma = cur->find_vma(addr, 0)) {
                vma->length = addr - vma->start;
                if (vma->length == 0) {
                    cur->remove_vma(vma->start, vma->length);
                }
            }
        }

        cur->heap_end = addr;
        return addr;
    }
}  // namespace syscalls::internal
