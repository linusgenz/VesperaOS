// vm.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.05.26.
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
#include <realm/address_space.h>
#include <realm/handle_table.h>
#include <realm/realm.h>
#include <units/unit.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/mm/shm.h>
#include <vespera/mm/vm.h>
#include <vespera/sys/mman.h>
#include <vespera_errno.h>

namespace kernel::vm {

    namespace {

        constexpr uptr MMAP_BASE = 0x0000'6000'0000'0000ULL;
        constexpr uptr MMAP_END = 0x0000'7FFF'FF00'0000ULL;
        constexpr uptr USER_HEAP_MAX = 0x0000'7FFE'0000'0000ULL;

        uptr find_free_range(const Unit* u, const usize length) {
            uptr current = MMAP_BASE;

            while (true) {
                uptr next_start = MMAP_END;

                for (const kernel::units::VmArea* v = u->get_vma_list(); v; v = v->next) {
                    if (v->start >= current && v->start < next_start) next_start = v->start;
                }

                if (current + length <= next_start) {
                    return (current + length <= MMAP_END) ? current : 0;
                }

                // Advance past the blocking VMA
                for (const kernel::units::VmArea* v = u->get_vma_list(); v; v = v->next) {
                    if (v->start == next_start) {
                        const uptr end = next_start + v->length;
                        current = (end + PAGE_SIZE - 1) & ~uptr(PAGE_SIZE - 1);
                        break;
                    }
                }

                if (current >= MMAP_END) return 0;
            }
        }

        PageTableManager* ptm_for(const Unit* u) {
            return u->parent->address_space->page_table();
        }

    }  // namespace

    i64 mmap(Unit* u, uptr addr, usize length, const u64 prot, const u64 flags, const u64 handle, const u64 offset) {
        if (length == 0) return -EINVAL;
        if (!u || !u->is_user) return -EACCES;

        addr = addr & ~uptr(PAGE_SIZE - 1);
        length = (length + PAGE_SIZE - 1) & ~usize(PAGE_SIZE - 1);
        const usize npages = length / PAGE_SIZE;

        const uptr base = (addr != 0) ? addr : find_free_range(u, length);
        if (base == 0) return -ENOMEM;

        PageTableManager* ptm = ptm_for(u);
        ShmObject* shm_obj = nullptr;

        if (!(flags & MAP_ANONYMOUS)) {
            HandleEntry* he = u->parent->handle_table->lookup(handle);
            if (!he || he->type != HANDLE_TYPE_SHM) return -EBADH;

            shm_obj = static_cast<ShmObject*>(he->resource);
            SpinlockGuard sg(shm_obj->lock);

            const usize shm_page_aligned_size = (shm_obj->size + PAGE_SIZE - 1) & ~usize(PAGE_SIZE - 1);
            if (offset + length > shm_page_aligned_size) return -EINVAL;  // Out of bounds
            shm_obj->mapping_count++;
        }

        for (usize i = 0; i < npages; i++) {
            phys_addr_t phys;
            if (flags & MAP_ANONYMOUS) {
                phys = kernel::memory::request_page_phys();
            } else {
                usize page_idx = (offset / PAGE_SIZE) + i;
                phys = shm_obj->pages[page_idx];

                if (phys_null(phys)) {
                    phys = kernel::memory::request_page_phys();
                    if (!phys_null(phys)) {
                        memset(phys_to_virt(phys), 0, PAGE_SIZE);
                        shm_obj->pages[page_idx] = phys;
                    }
                }
            }

            if (phys_null(phys)) {
                for (usize j = 0; j < i; j++) {
                    ptm->unmap_memory(virt_from_raw(base + j * PAGE_SIZE));
                }
                if (shm_obj) {
                    SpinlockGuard sg(shm_obj->lock);
                    shm_obj->mapping_count--;
                }
                return -ENOMEM;
            }

            u64 pt_flags = (1ULL << UserSuper);
            if (prot & PROT_WRITE) pt_flags |= (1ULL << ReadWrite);
            ptm->map_memory(virt_from_raw(base + i * PAGE_SIZE), phys, pt_flags);
        }

        auto* area = static_cast<kernel::units::VmArea*>(kernel::memory::malloc(sizeof(kernel::units::VmArea)));
        if (!area) {
            for (usize i = 0; i < npages; i++) ptm->unmap_memory(virt_from_raw(base + i * PAGE_SIZE));
            if (shm_obj) {
                SpinlockGuard sg(shm_obj->lock);
                shm_obj->mapping_count--;
            }
            return -ENOMEM;
        }

        area->start = base;
        area->length = length;
        area->prot = prot;
        area->flags = flags;
        area->file_off = offset;
        area->handle = handle;
        area->backing_obj = shm_obj;
        u->add_vma(area);

        return static_cast<i64>(base);
    }

    i64 brk(Unit* u, const uptr addr) {
        if (!u || !u->is_user) return -EACCES;
        if (u->heap_start == 0) return -EINVAL;
        if (addr == 0) return static_cast<i64>(u->heap_end);
        if (addr < u->heap_start) return static_cast<i64>(u->heap_end);
        if (addr > USER_HEAP_MAX) return -ENOMEM;

        PageTableManager* ptm = ptm_for(u);

        if (addr > u->heap_end) {
            const uptr start = (u->heap_end + 0xFFF) & ~0xFFFULL;
            const uptr end = (addr + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += PAGE_SIZE) {
                const phys_addr_t phys = kernel::memory::request_page_phys();
                if (phys_null(phys)) return -ENOMEM;
                memset(phys_to_virt(phys), 0, PAGE_SIZE);
                ptm->map_memory(
                    virt_from_raw(a),
                    phys,
                    (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
                );
            }

            if (kernel::units::VmArea* existing = u->find_vma(u->heap_end, 0)) {
                existing->length = addr - existing->start;
            } else {
                auto* new_vma = new kernel::units::VmArea();
                new_vma->start = u->heap_end;
                new_vma->length = addr - u->heap_end;
                new_vma->prot = 0;
                new_vma->flags = 0;
                new_vma->file_off = 0;
                new_vma->handle = 0;
                u->add_vma(new_vma);
            }
        } else {
            const uptr start = (addr + 0xFFF) & ~0xFFFULL;
            const uptr end = (u->heap_end + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += PAGE_SIZE) {
                const virt_addr_t vaddr = virt_from_raw(a);
                if (const phys_addr_t phys = ptm->get_physical_address(vaddr); !phys_null(phys)) {
                    ptm->unmap_memory(vaddr);
                    kernel::memory::free_page_phys(phys);
                }
            }

            if (kernel::units::VmArea* vma = u->find_vma(addr, 0)) {
                vma->length = addr - vma->start;
                if (vma->length == 0) u->remove_vma(vma->start, 0);
            }
        }

        u->heap_end = addr;
        return static_cast<i64>(addr);
    }

    i64 munmap(Unit* u, const uptr addr, usize length) {
        if (!u || !u->is_user) return -EACCES;
        if (length == 0) return -EINVAL;

        const uptr base = addr & ~uptr(PAGE_SIZE - 1);
        const usize len = (length + PAGE_SIZE - 1) & ~usize(PAGE_SIZE - 1);
        const uptr end = base + len;

        PageTableManager* ptm = ptm_for(u);

        kernel::units::VmArea* split_tail = nullptr;
        kernel::units::VmArea* vma_to_split = nullptr;

        kernel::units::VmArea* vma = u->get_vma_list();
        while (vma) {
            const uptr vma_end = vma->start + vma->length;

            if (vma->start < base && vma_end > end) {
                split_tail = static_cast<kernel::units::VmArea*>(kernel::memory::malloc(sizeof(kernel::units::VmArea)));

                if (!split_tail) {
                    return -ENOMEM;
                }
                vma_to_split = vma;
                break;
            }
            vma = vma->next;
        }

        vma = u->get_vma_list();
        while (vma) {
            kernel::units::VmArea* const next_vma = vma->next;
            const uptr vma_end = vma->start + vma->length;

            if (vma->start >= end || vma_end <= base) {
                vma = next_vma;
                continue;
            }

            const uptr unmap_start = (vma->start > base) ? vma->start : base;
            const uptr unmap_end = (vma_end < end) ? vma_end : end;

            for (uptr a = unmap_start; a < unmap_end; a += PAGE_SIZE) {
                const virt_addr_t va = virt_from_raw(a);
                if (const phys_addr_t phys = ptm->get_physical_address(va); !phys_null(phys)) {
                    ptm->unmap_memory(va);

                    if (vma->flags & MAP_ANONYMOUS) {
                        kernel::memory::free_page_phys(phys);
                    }
                }
            }

            if (vma->start >= base && vma_end <= end) {
                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    auto* shm = static_cast<ShmObject*>(vma->backing_obj);
                    shm->release_mapping();  // Benachrichtige SHM-Subsystem
                }
                u->remove_vma(vma->start, vma->length);
            } else if (vma == vma_to_split) {
                split_tail->start = end;
                split_tail->length = vma_end - end;
                split_tail->prot = vma->prot;
                split_tail->flags = vma->flags;
                split_tail->file_off = vma->file_off + (end - vma->start);
                split_tail->handle = vma->handle;
                split_tail->backing_obj = vma->backing_obj;

                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    auto* shm = static_cast<ShmObject*>(vma->backing_obj);
                    SpinlockGuard sg(shm->lock);
                    shm->mapping_count++;
                }

                u->add_vma(split_tail);
                vma->length = base - vma->start;
            } else if (vma->start >= base) {
                const usize diff = end - vma->start;
                vma->length = vma_end - end;
                vma->start = end;
                vma->file_off += diff;
            } else {
                vma->length = base - vma->start;
            }

            vma = next_vma;
        }

        return 0;
    }

}  // namespace kernel::vm