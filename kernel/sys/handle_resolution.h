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
#include <realm/handle_table.h>
#include <uapi/vespera/capabilities.h>
#include <uapi/vespera/handles.h>
#include <vespera/types.h>

class Realm;

namespace syscalls {

    /**
     * @brief Validated, ready-to-use handle resolved from the current realm.
     *
     * Syscall handlers receive this from @ref resolve_handle and should access
     * the underlying resource exclusively through the provided accessors.
     * Direct field access to the internal HandleEntry is intentionally prevented.
     */
    class ResolvedHandle {
       public:
        ResolvedHandle() = default;

        // Realm is intentionally accessible — some syscalls need it for
        // secondary handle operations (transfer, close, spawn inheritance).
        Realm* realm{nullptr};

        [[nodiscard]] u64 type() const {
            return entry_->type & HANDLE_TYPE_MASK;
        }

        [[nodiscard]] capability_set capabilities() const {
            return entry_->capabilities;
        }

        [[nodiscard]] bool transferable() const {
            return entry_->transferable;
        }

        [[nodiscard]] HandleId hid() const {
            return entry_->hid;
        }

        template <typename T>
        [[nodiscard]] T* resource_as() const {
            return static_cast<T*>(entry_->resource);
        }

        void release() const;

       private:
        friend Result<ResolvedHandle> resolve_handle(HandleId, u64, capability_set);
        HandleEntry* entry_{nullptr};

        ResolvedHandle(Realm* r, HandleEntry* e)
            : realm(r)
            , entry_(e) {
        }
    };

    /**
     * @brief Resolves and validates a handle from the current realm.
     *
     * @param hid           Handle ID from userspace.
     * @param type_mask     Required type; 0 skips the type check.
     * @param required_caps Capability bits that must ALL be present; 0 skips.
     *
     * @return ResolvedHandle on success.
     * @return Error::Srch   if no realm is active.
     * @return Error::BadH   if the handle is not found.
     * @return Error::Acces  if capability check fails.
     * @return Error::NotTty if type_mask is set but does not match.
     */
    [[nodiscard]] Result<ResolvedHandle> resolve_handle(
        HandleId hid, u64 type_mask = 0, capability_set required_caps = 0
    );

}  // namespace syscalls

#endif  // VESPERAOS_KERNEL_SYS_HANDLE_RESOLUTION_H