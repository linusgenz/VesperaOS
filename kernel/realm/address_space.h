// address_space.h
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

#ifndef VESPERAOS_KERNEL_REALM_ADDRESS_SPACE_H
#define VESPERAOS_KERNEL_REALM_ADDRESS_SPACE_H

#include <vespera/mm/addr.h>
#include <vespera/realm/user_stack_allocator.h>
#include <vespera/types.h>

#include "../paging/page_table_manager.h"
#include "vm_area_list.h"

namespace kernel::realm {

    // AddressSpace owns the virtual address space of a user realm:
    //   - the PML4 physical page
    //   - the PageTableManager that manipulates it
    //   - the UserStackAllocator for unit stacks
    //   - trampoline mapping
    //
    // Kernel realms (is_user == false) never create an AddressSpace.
    class AddressSpace {
    public:
        AddressSpace() = default;
        ~AddressSpace() = default;

        AddressSpace(const AddressSpace&) = delete;
        AddressSpace& operator=(const AddressSpace&) = delete;

        [[nodiscard]] i64 init(phys_addr_t trampoline_page, usize stack_slot_size);

        void destroy();

        [[nodiscard]] phys_addr_t pml4_phys() const { return pml4_phys_; }
        [[nodiscard]] PageTableManager* page_table() const { return page_table_; }
        [[nodiscard]] UserStackAllocator& stack_alloc() { return stack_alloc_; }

        [[nodiscard]] kernel::units::VmAreaList& vma_list() { return vm_areas_; }
        [[nodiscard]] Spinlock& vm_lock() { return vm_lock_; }

        [[nodiscard]] uptr heap_start() const { return heap_start_; }
        [[nodiscard]] uptr heap_end() const { return heap_end_; }
        void set_heap_start(uptr v) { heap_start_ = v; }
        void set_heap_end(uptr v) { heap_end_ = v; }

    private:
        void map_trampoline(phys_addr_t trampoline_page) const;
        void release_vm_areas();

        phys_addr_t pml4_phys_{};
        PageTableManager* page_table_{nullptr};
        UserStackAllocator stack_alloc_{};

        kernel::units::VmAreaList vm_areas_;
        Spinlock vm_lock_;
        uptr heap_start_{0};
        uptr heap_end_{0};
    };

} // namespace kernel::realm

#endif // VESPERAOS_KERNEL_REALM_ADDRESS_SPACE_H
