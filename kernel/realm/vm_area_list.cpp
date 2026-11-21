// vm_area_list.cpp
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

#include "vm_area_list.h"

namespace kernel::units {

    void VmAreaList::add(VmArea* vma) {
        vma->next = head_;
        head_     = vma;
    }

    VmArea* VmAreaList::find(const uptr addr, const usize len) const {
        for (VmArea* v = head_; v; v = v->next) {
            if (addr >= v->start && (addr + len) <= (v->start + v->length))
                return v;
        }
        return nullptr;
    }

    bool VmAreaList::remove(const uptr addr, const usize len) {
        VmArea* prev = nullptr;
        VmArea* cur  = head_;

        while (cur) {
            if (cur->start == addr && cur->length == len) {
                if (prev)
                    prev->next = cur->next;
                else
                    head_ = cur->next;
                delete cur;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    void VmAreaList::free_all() {
        VmArea* cur = head_;
        while (cur) {
            VmArea* next = cur->next;
            delete cur;
            cur = next;
        }
        head_ = nullptr;
    }

} // namespace kernel::units
