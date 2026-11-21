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

#include <filesystem/devfs.h>
#include <filesystem/vfs_handle.h>
#include <filesystem/vfs_node.h>
#include <klib/string.h>
#include <realm/address_space.h>
#include <realm/handle_table.h>
#include <realm/realm.h>
#include <units/unit.h>
#include <vespera/devices/char_device.h>
#include <vespera/mm/file_backing.h>
#include <vespera/mm/memory.h>
#include <vespera/mm/shm.h>
#include <vespera/mm/vm.h>
#include <vespera/sys/mman.h>
#include <vespera_errno.h>

#include "vespera/mm/dmabuf.h"

namespace kernel::vm {
    namespace {
        constexpr uptr MMAP_BASE = 0x0000'6000'0000'0000ULL;
        constexpr uptr MMAP_END = 0x0000'7FFF'FF00'0000ULL;
        constexpr uptr USER_HEAP_MAX = 0x0000'7FFE'0000'0000ULL;

        uptr find_free_range(kernel::realm::AddressSpace* as, const usize length) {
            uptr current = MMAP_BASE;

            while (current + length <= MMAP_END) {
                bool overlap = false;
                for (const kernel::units::VmArea* v = as->vma_list().head(); v; v = v->next) {
                    const uptr v_start = v->start;
                    const uptr v_end = (v->start + v->length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                    if (v_end > current && v_start < current + length) {
                        current = v_end;
                        overlap = true;
                        break;
                    }
                }
                if (!overlap) return current;
            }
            return 0;
        }

        PageTableManager* ptm_for(const Unit* u) {
            return u->parent->address_space->page_table();
        }

        kernel::realm::AddressSpace* as_for(const Unit* u) {
            return u->parent->address_space;
        }
    } // namespace

    i64 mmap(Unit* u, uptr addr, usize length, const u64 prot, const u64 flags, const u64 handle, const u64 offset) {
        if (length == 0) return -EINVAL;
        if (!u || !u->is_user) return -EACCES;

        addr = addr & ~(PAGE_SIZE - 1);
        length = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const usize npages = length / PAGE_SIZE;

        auto* as = as_for(u);
        PageTableManager* ptm = as->page_table();

        kernel::vm::VmBackingObject* backing_obj = nullptr;
        u64 hdl_type = 0;

        if (!(flags & MAP_ANONYMOUS)) {
            HandleEntry* he = u->parent->handle_table->lookup(handle);
            if (!he) return -EBADH;
            hdl_type = he->type;

            if (hdl_type == HANDLE_TYPE_SHM) {
                backing_obj = static_cast<ShmObject*>(he->resource);
            } else if (hdl_type == HANDLE_TYPE_FILE) {
                const auto* vfs_handle = static_cast<VfsHandle*>(he->resource);
                if (!vfs_handle || !vfs_handle->node) return -EBADH;
                backing_obj = FileBackingObject::get_or_create(vfs_handle->node);
            } else if (hdl_type == HANDLE_TYPE_DEVICE) {
                const auto* vfs_handle = static_cast<VfsHandle*>(he->resource);
                if (!vfs_handle || !vfs_handle->node) return -EBADH;

                CharFile* cf = DevFs::get_char_file(vfs_handle->context);
                const auto* entry = static_cast<DevfsEntry*>(vfs_handle->node->internal_data);
                if (!cf || !entry || !entry->device || !entry->device->chardev) return -EBADH;

                backing_obj = entry->device->chardev->get_backing_object(cf, offset);
            } else if (hdl_type == HANDLE_TYPE_DMABUF) {
                backing_obj = static_cast<DmaBuf*>(he->resource);
            } else {
                return -EBADH;
            }

            if (!backing_obj) return -EBADH;

            backing_obj->add_mapping();

            const usize backing_size = backing_obj->get_size();
            const usize backing_page_aligned_size = (backing_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

            if (hdl_type == HANDLE_TYPE_DEVICE) {
                if (length > backing_page_aligned_size) {
                    backing_obj->remove_mapping();
                    return -EINVAL;
                }
            } else if (offset > backing_page_aligned_size || length > backing_page_aligned_size - offset) {
                backing_obj->remove_mapping();
                return -EINVAL;
            }
        }

        SpinlockGuard guard(as->vm_lock());

        uptr base = 0;
        if (addr != 0) {
            if (flags & MAP_FIXED) {
                base = addr;
                // TODO: implement correct MAP_FIXED
            } else {
                bool addr_free = true;
                for (const kernel::units::VmArea* v = as->vma_list().head(); v; v = v->next) {
                    const uptr v_end = (v->start + v->length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                    if (v_end > addr && v->start < addr + length) {
                        addr_free = false;
                        break;
                    }
                }
                base = addr_free ? addr : find_free_range(as, length);
            }
        } else {
            base = find_free_range(as, length);
        }

        if (base == 0) {
            if (backing_obj) backing_obj->remove_mapping();
            return -ENOMEM;
        }

        const CacheMode cache_mode = backing_obj ? backing_obj->get_cache_mode() : CacheMode::WriteBack;
        const usize page_offset = (hdl_type == HANDLE_TYPE_DEVICE) ? 0 : offset;

        for (usize i = 0; i < npages; i++) {
            phys_addr_t phys;
            if (flags & MAP_ANONYMOUS) {
                phys = kernel::memory::request_page_phys();
                if (!phys_null(phys)) {
                    memset(phys_to_virt(phys), 0, PAGE_SIZE);
                }
            } else {
                phys = backing_obj->get_page(page_offset + i * PAGE_SIZE);
            }

            if (phys_null(phys)) {
                for (usize j = 0; j < i; j++) ptm->unmap_memory(virt_from_raw(base + j * PAGE_SIZE));
                if (backing_obj) backing_obj->remove_mapping();
                return -ENOMEM;
            }

            u64 pt_flags = (1ULL << UserSuper);
            if (prot & PROT_WRITE) pt_flags |= (1ULL << ReadWrite);
            ptm->map_memory(virt_from_raw(base + i * PAGE_SIZE), phys, pt_flags, cache_mode);
        }

        auto* area = new kernel::units::VmArea{};
        if (!area) {
            for (usize i = 0; i < npages; i++) ptm->unmap_memory(virt_from_raw(base + i * PAGE_SIZE));
            if (backing_obj) backing_obj->remove_mapping();
            return -ENOMEM;
        }

        area->start = base;
        area->length = length;
        area->prot = prot;
        area->flags = flags;
        area->file_off = offset;
        area->handle = handle;
        area->backing_obj = backing_obj;
        as->vma_list().add(area);

        return static_cast<i64>(base);
    }

    i64 brk(Unit* u, const uptr addr) {
        if (!u || !u->is_user) return -EACCES;

        auto* as = as_for(u);
        SpinlockGuard guard(as->vm_lock());

        if (as->heap_start() == 0) return -EINVAL;
        if (addr == 0) return static_cast<i64>(as->heap_end());
        if (addr < as->heap_start()) return static_cast<i64>(as->heap_end());
        if (addr > USER_HEAP_MAX) return -ENOMEM;

        PageTableManager* ptm = as->page_table();

        if (addr > as->heap_end()) {
            const uptr start = (as->heap_end() + 0xFFF) & ~0xFFFULL;
            const uptr end = (addr + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += PAGE_SIZE) {
                const phys_addr_t phys = kernel::memory::request_page_phys();
                if (phys_null(phys)) return -ENOMEM;
                memset(phys_to_virt(phys), 0, PAGE_SIZE);
                ptm->map_memory(
                    virt_from_raw(a), phys,
                    (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::UserSuper)
                );
            }

            kernel::units::VmArea* existing = nullptr;
            for (auto* v = as->vma_list().head(); v; v = v->next) {
                if (v->start == as->heap_end()) {
                    existing = v;
                    break;
                }
            }
            if (existing) {
                existing->length = addr - existing->start;
            } else {
                auto* new_vma = new kernel::units::VmArea();
                new_vma->start = as->heap_end();
                new_vma->length = addr - as->heap_end();
                new_vma->prot = 0;
                new_vma->flags = 0;
                new_vma->file_off = 0;
                new_vma->handle = 0;
                as->vma_list().add(new_vma);
            }
        } else {
            const uptr start = (addr + 0xFFF) & ~0xFFFULL;
            const uptr end = (as->heap_end() + 0xFFF) & ~0xFFFULL;

            for (uptr a = start; a < end; a += PAGE_SIZE) {
                const virt_addr_t vaddr = virt_from_raw(a);
                if (const phys_addr_t phys = ptm->get_physical_address(vaddr); !phys_null(phys)) {
                    ptm->unmap_memory(vaddr);
                    kernel::memory::free_page_phys(phys);
                }
            }

            if (kernel::units::VmArea* vma = as->vma_list().find(addr, 0)) {
                vma->length = addr - vma->start;
                if (vma->length == 0) as->vma_list().remove(vma->start, 0);
            }
        }

        as->set_heap_end(addr);
        return static_cast<i64>(addr);
    }

    i64 munmap(Unit* u, const uptr addr, usize length) {
        if (!u || !u->is_user) return -EACCES;
        if (length == 0) return -EINVAL;

        const uptr base = addr & ~(PAGE_SIZE - 1);
        const usize len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const uptr end = base + len;

        auto* as = as_for(u);
        SpinlockGuard guard(as->vm_lock());

        PageTableManager* ptm = as->page_table();

        kernel::units::VmArea* split_tail = nullptr;
        kernel::units::VmArea* vma_to_split = nullptr;

        kernel::units::VmArea* vma = as->vma_list().head();
        while (vma) {
            const uptr vma_end = vma->start + vma->length;

            if (vma->start < base && vma_end > end) {
                split_tail = new kernel::units::VmArea{};
                if (!split_tail) return -ENOMEM;
                vma_to_split = vma;
                break;
            }
            vma = vma->next;
        }

        vma = as->vma_list().head();
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
                if (const phys_addr_t phys = ptm->get_physical_address_ignore_present(va); !phys_null(phys)) {
                    ptm->unmap_memory(va);

                    if (vma->flags & MAP_ANONYMOUS) {
                        kernel::memory::free_page_phys(phys);
                    }
                }
            }

            if (vma->start >= base && vma_end <= end) {
                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    vma->backing_obj->remove_mapping();
                }
                as->vma_list().remove(vma->start, vma->length);
            } else if (vma == vma_to_split) {
                split_tail->start = end;
                split_tail->length = vma_end - end;
                split_tail->prot = vma->prot;
                split_tail->flags = vma->flags;
                split_tail->file_off = vma->file_off + (end - vma->start);
                split_tail->handle = vma->handle;
                split_tail->backing_obj = vma->backing_obj;

                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    vma->backing_obj->add_mapping();
                }

                as->vma_list().add(split_tail);
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

    i64 mprotect(Unit* u, const uptr addr, usize length, const u64 prot) {
        if (!u || !u->is_user) return -EACCES;
        if (length == 0) return -EINVAL;

        const uptr base = addr & ~(PAGE_SIZE - 1);
        const usize len = (length + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        const uptr end = base + len;

        auto* as = as_for(u);
        SpinlockGuard guard(as->vm_lock());

        PageTableManager* ptm = as->page_table();

        // Pass 1: Ensure full VMA coverage (holes return -ENOMEM)
        {
            uptr covered = base;
            while (covered < end) {
                kernel::units::VmArea* hit = nullptr;
                for (kernel::units::VmArea* it = as->vma_list().head(); it; it = it->next) {
                    if (it->start <= covered && it->start + it->length > covered) {
                        hit = it;
                        break;
                    }
                }
                if (!hit) return -ENOMEM;
                covered = hit->start + hit->length;
            }
        }

        kernel::units::VmArea* vma = as->vma_list().head();
        while (vma) {
            kernel::units::VmArea* const next_vma = vma->next;
            const uptr vma_start = vma->start;
            const uptr vma_end = vma->start + vma->length;

            if (vma_start >= end || vma_end <= base) {
                vma = next_vma;
                continue;
            }

            const uptr change_start = (vma_start > base) ? vma_start : base;
            const uptr change_end = (vma_end < end) ? vma_end : end;

            const CacheMode cache_mode = vma->backing_obj
                                             ? vma->backing_obj->get_cache_mode()
                                             : CacheMode::WriteBack;

            // Remap mapped pages. PROT_NONE clears Present bit to keep phys mapping without access.
            // ignore_present lookup is required to locate existing PROT_NONE pages.
            const bool make_present = (prot != PROT_NONE);
            for (uptr a = change_start; a < change_end; a += PAGE_SIZE) {
                const virt_addr_t va = virt_from_raw(a);
                if (const phys_addr_t phys = ptm->get_physical_address_ignore_present(va); !phys_null(phys)) {
                    u64 pt_flags = (1ULL << UserSuper);
                    if (prot & PROT_WRITE) pt_flags |= (1ULL << ReadWrite);
                    ptm->map_memory(va, phys, pt_flags, cache_mode, make_present);
                }
            }

            // Create tail VMA [change_end, vma_end) with old prot if needed
            if (change_end < vma_end) {
                auto* tail = new kernel::units::VmArea{};
                if (!tail) return -ENOMEM;

                tail->start = change_end;
                tail->length = vma_end - change_end;
                tail->prot = vma->prot;
                tail->flags = vma->flags;
                tail->file_off = vma->file_off + (change_end - vma_start);
                tail->handle = vma->handle;
                tail->backing_obj = vma->backing_obj;

                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    vma->backing_obj->add_mapping();
                }

                as->vma_list().add(tail);
            }

            // Split head: insert middle VMA for new prot and shrink original VMA to leading remainder
            if (change_start > vma_start) {
                auto* mid = new kernel::units::VmArea{};
                if (!mid) return -ENOMEM;

                mid->start = change_start;
                mid->length = change_end - change_start;
                mid->prot = prot;
                mid->flags = vma->flags;
                mid->file_off = vma->file_off + (change_start - vma_start);
                mid->handle = vma->handle;
                mid->backing_obj = vma->backing_obj;

                if (!(vma->flags & MAP_ANONYMOUS) && vma->backing_obj) {
                    vma->backing_obj->add_mapping();
                }

                as->vma_list().add(mid);

                vma->length = change_start - vma_start;
            } else {
                // Update VMA in place if no leading split is required
                vma->length = change_end - vma_start;
                vma->prot = prot;
            }

            vma = next_vma;
        }

        return 0;
    }
} // namespace kernel::vm
