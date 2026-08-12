// vm_area_list.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.05.26.
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


#ifndef VESPERAOS_KERNEL_UNITS_VM_AREA_LIST_H
#define VESPERAOS_KERNEL_UNITS_VM_AREA_LIST_H

#include <vespera/types.h>

#include "vespera/mm/vm_backing.h"

namespace kernel::units {

// VmArea — describes a single mapped virtual memory region for a unit.
// Owned by VmAreaList.
struct VmArea {
    uptr     start;
    usize    length;
    u64      prot;
    u64      flags;
    uptr     file_off;
    HandleId handle;
    vm::VmBackingObject* backing_obj;
    VmArea*  next;
};

// VmAreaList — intrusive singly-linked list of VmArea entries.
//
// Ownership: the list owns every VmArea it holds and frees them on
// free_all(). Callers must heap-allocate VmArea before passing to add().
//
// Thread safety: none — the caller (Unit) is responsible for any locking.
class VmAreaList {
public:
    VmAreaList() = default;
    ~VmAreaList() = default;

    VmAreaList(const VmAreaList&)            = delete;
    VmAreaList& operator=(const VmAreaList&) = delete;

    // Prepend vma to the list. Takes ownership.
    void add(VmArea* vma);

    // Find the first area that fully contains [addr, addr+len).
    // Returns nullptr if no such area exists.
    [[nodiscard]] VmArea* find(uptr addr, usize len) const;

    // Remove and delete the area at exactly (start=addr, length=len).
    // Returns true if found and removed.
    bool remove(uptr addr, usize len);

    void free_all();

    [[nodiscard]] VmArea* head() const { return head_; }
    [[nodiscard]] bool    empty() const { return head_ == nullptr; }

private:
    VmArea* head_{nullptr};
};

} // namespace kernel::units

#endif // VESPERAOS_KERNEL_UNITS_VM_AREA_LIST_H
