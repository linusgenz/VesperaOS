// address_space.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.05.26.
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

#include "address_space.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/addr.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_config.h>
#include <vespera/realm/realm_types.h>
#include <vespera/types.h>
#include <vespera_errno.h>

#include "../paging/page_table_manager.h"
#include "../paging/paging.h"

namespace kernel::realm {

i64 AddressSpace::init(phys_addr_t trampoline_page, usize stack_slot_size) {
    pml4_phys_ = kernel::memory::request_page_phys();
    if (phys_null(pml4_phys_)) {
        Log::error("AddressSpace::init: failed to allocate PML4 page");
        return -ENOMEM;
    }

    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys_)));
    memset(pml4_virt, 0, PAGE_SIZE);

    const auto* kernel_pml4 = static_cast<const PageTable*>(
        virt_ptr(phys_to_virt(make_phys(kernel::memory::get_pagetable_address())))
    );
    for (int i = 256; i < 512; ++i) {
        pml4_virt->entries[i] = kernel_pml4->entries[i];
    }

    page_table_ = new PageTableManager(reinterpret_cast<PageTable*>(phys_raw(pml4_phys_)));
    if (!page_table_) {
        kernel::memory::free_page_phys(pml4_phys_);
        pml4_phys_ = {};
        return -ENOMEM;
    }

    map_trampoline(trampoline_page);

    stack_alloc_.init(stack_slot_size);

    return SUCCESS_CODE;
}

void AddressSpace::destroy() {
    if (!page_table_) return;

    page_table_->destroy_userspace();

    delete page_table_;
    page_table_ = nullptr;
    pml4_phys_  = {};
}

void AddressSpace::map_trampoline(phys_addr_t trampoline_page) const {
    page_table_->map_range(
        virt_from_raw(TRAMPOLINE_VADDR),
        trampoline_page,
        PAGE_SIZE,
        (1ULL << PtFlag::Present) | (1ULL << PtFlag::UserSuper)
    );
}

} // namespace kernel::realm
