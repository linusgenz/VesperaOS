// handle_resolution.h
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

#ifndef VESPERAOS_KERNEL_SYS_HANDLE_RESOLUTION_H
#define VESPERAOS_KERNEL_SYS_HANDLE_RESOLUTION_H

#include <klib/result.h>
#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/handles.h>
#include <vespera/types.h>

#include <realm/handle_table.h>

class Unit;
class Realm;

namespace syscalls {

struct ResolvedHandle {
    Realm*        realm;
    HandleEntry*  entry;

    [[nodiscard]] u64 type() const;

    template<typename T>
    [[nodiscard]] T* resource_as() const;
};

// resolve_handle — the single entry point for all syscall handle validation.
//
//   hid           — handle ID from userspace
//   type_mask     — if non-zero, entry->type & HANDLE_TYPE_MASK must equal this;
//                   pass 0 to skip the type check and dispatch yourself
//   required_caps — capability bits that must ALL be present; pass 0 for no check
//
// Returns:
//   Result::ok(ResolvedHandle)  — ready to use
//   Error::Srch   — no current unit / unit not active
//   Error::BadH   — handle not found
//   Error::Acces  — capability check failed
//   Error::NotTty — type_mask given but type does not match
[[nodiscard]] Result<ResolvedHandle> resolve_handle(
    HandleId       hid,
    u64            type_mask     = 0,
    capability_set required_caps = 0
);

inline u64 ResolvedHandle::type() const {
    return entry->type & HANDLE_TYPE_MASK;
}

template<typename T>
inline T* ResolvedHandle::resource_as() const {
    return static_cast<T*>(entry->resource);
}

} // namespace syscalls

#endif // VESPERAOS_KERNEL_SYS_HANDLE_RESOLUTION_H
