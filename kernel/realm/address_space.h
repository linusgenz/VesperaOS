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

class PageTableManager;

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

    private:
        void map_trampoline(phys_addr_t trampoline_page);

        phys_addr_t pml4_phys_{};
        PageTableManager* page_table_{nullptr};
        UserStackAllocator stack_alloc_{};
    };

} // namespace kernel::realm

#endif // VESPERAOS_KERNEL_REALM_ADDRESS_SPACE_H
