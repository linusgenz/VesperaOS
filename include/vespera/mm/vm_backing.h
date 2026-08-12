// vm_backing.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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

#ifndef VESPERAOS_VM_BACKING_H
#define VESPERAOS_VM_BACKING_H

#include <vespera/types.h>
#include <vespera/mm/addr.h>

namespace kernel::vm {

    class VmBackingObject {
    public:
        virtual ~VmBackingObject() = default;

        virtual phys_addr_t get_page(usize offset_in_bytes) = 0;

        virtual void sync_page(usize offset_in_bytes, phys_addr_t phys, bool is_dirty) {
            (void)offset_in_bytes; (void)phys; (void)is_dirty;
        }

        [[nodiscard]] virtual usize get_size() const = 0;

        virtual void add_mapping() = 0;
        virtual void remove_mapping() = 0;
    };

}

#endif //VESPERAOS_VM_BACKING_H
