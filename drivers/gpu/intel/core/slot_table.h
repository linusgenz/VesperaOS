// slot_table.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 19.09.26.
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

#ifndef VESPERAOS_SLOT_TABLE_H
#define VESPERAOS_SLOT_TABLE_H

#include <vespera/types.h>

#include "klib/vector.h"

template <typename T>
class SlotTable {
public:
    explicit SlotTable(usize initial_capacity = 0) : slots_(initial_capacity) {
    }

    SlotTable(const SlotTable&) = delete;
    SlotTable& operator=(const SlotTable&) = delete;

    [[nodiscard]] u32 insert(const T& value) {
        if (!free_indices_.empty()) {
            const usize index = free_indices_.back();
            free_indices_.pop_back();
            slots_[index] = value;
            return static_cast<u32>(index) + 1;
        }

        slots_.push_back(value);
        return static_cast<u32>(slots_.size());
    }

    bool remove(const u32 handle, const T& empty_value = T{}) {
        if (handle == 0 || handle > slots_.size()) {
            return false;
        }
        const usize index = handle - 1;
        slots_[index] = empty_value;
        free_indices_.push_back(index);
        return true;
    }

    [[nodiscard]] T* get(const u32 handle) {
        if (handle == 0 || handle > slots_.size()) {
            return nullptr;
        }
        return &slots_[handle - 1];
    }

    [[nodiscard]] const T* get(const u32 handle) const {
        if (handle == 0 || handle > slots_.size()) {
            return nullptr;
        }
        return &slots_[handle - 1];
    }

    /// Highest handle value ever handed out (== current backing size).
    /// Some free indices below this may be unused -- this is an upper
    /// bound for iteration, not a live-entry count.
    [[nodiscard]] usize capacity() const {
        return slots_.size();
    }

    [[nodiscard]] bool has_free_slot() const {
        return !free_indices_.empty();
    }

    T* begin() { return slots_.data(); }
    T* end() { return slots_.data() + slots_.size(); }
    const T* begin() const { return slots_.data(); }
    const T* end() const { return slots_.data() + slots_.size(); }

private:
    Vector<T> slots_;
    Vector<usize> free_indices_;
};

#endif  // VESPERAOS_SLOT_TABLE_H