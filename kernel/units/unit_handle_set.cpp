// unit_handle_set.cpp
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

#include "unit_handle_set.h"

#include <vespera_errno.h>

namespace kernel::units {

    void UnitHandleSet::init() {
        lock_.init();
    }

    i64 UnitHandleSet::attach(const HandleId hid) {
        SpinlockGuard guard(lock_);

        if (count_ >= CAPACITY)
            return -ENOMEM;

        for (HandleId& slot : slots_) {
            if (slot == 0) {
                slot = hid;
                ++count_;
                return SUCCESS_CODE;
            }
        }
        return -ENOMEM;
    }

    i64 UnitHandleSet::detach(const HandleId hid) {
        SpinlockGuard guard(lock_);

        for (HandleId& slot : slots_) {
            if (slot == hid) {
                slot = 0;
                --count_;
                return SUCCESS_CODE;
            }
        }
        return -EBADH;
    }

    void UnitHandleSet::detach_all() {
        SpinlockGuard guard(lock_);

        for (HandleId& slot : slots_) slot = 0;
        count_ = 0;
    }

    u32 UnitHandleSet::find_slot(const HandleId hid) const {
        for (u32 i = 0; i < CAPACITY; ++i) {
            if (slots_[i] == hid) return i;
        }
        return static_cast<u32>(-1);
    }

} // namespace kernel::units

