// unit_handle_set.h
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

#ifndef VESPERAOS_KERNEL_UNITS_UNIT_HANDLE_SET_H
#define VESPERAOS_KERNEL_UNITS_UNIT_HANDLE_SET_H

#include <uapi/vespera/handles.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

namespace kernel::units {

    // UnitHandleSet — the set of HandleIds currently held by a single unit.
    //
    // This is NOT the Realm-level HandleTable (which owns the actual resources
    // and ref-counts). This is a lightweight per-unit index of which handles
    // that unit has opened, used to release them on unit teardown.
    //
    // Capacity is intentionally small (UNIT_MAX_HANDLES). If a unit needs more,
    // the limit should be raised here and nowhere else.
    //
    // Thread safety: internally locked via Spinlock.
    // Call init() once before first use.
    class UnitHandleSet {
       public:
        static constexpr usize CAPACITY = 64;

        UnitHandleSet() = default;

        UnitHandleSet(const UnitHandleSet&) = delete;
        UnitHandleSet& operator=(const UnitHandleSet&) = delete;

        // Must be called once after the containing Unit is allocated / memset-zeroed.
        void init();

        // Record that this unit holds hid. Returns 0 on success, -ENOMEM if full.
        [[nodiscard]] i64 attach(HandleId hid);

        // Remove hid from the set. Returns 0 on success, -EBADH if not found.
        [[nodiscard]] i64 detach(HandleId hid);

        // Remove every handle from the set (does NOT call HandleTable::release).
        void detach_all();

        // Returns the slot index of hid, or static_cast<u32>(-1) if not present.
        [[nodiscard]] u32 find_slot(HandleId hid) const;

        [[nodiscard]] u64 count() const {
            return count_;
        }

       private:
        HandleId slots_[CAPACITY]{};
        u64 count_{0};
        Spinlock lock_;
    };

}  // namespace kernel::units

#endif  // VESPERAOS_KERNEL_UNITS_UNIT_HANDLE_SET_H