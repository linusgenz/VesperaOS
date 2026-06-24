// sys_dup.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.06.26.
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


#include <realm/handle_table.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

static constexpr HandleId K_INVALID_HANDLE = -1ULL;

namespace syscalls::internal {

    static i64 do_dup(const HandleId src_hid, const HandleId dst_hid, const u32 flags) {
        const Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        HandleTable* ht = realm->handle_table;

        const HandleEntry* src = ht->lookup(src_hid);
        if (!src) return -EBADH;

        if (dst_hid != K_INVALID_HANDLE && dst_hid == src_hid)
            return static_cast<i64>(src_hid);

        if (src->acquire && src->resource)
            src->acquire(src->resource);

        if (dst_hid != K_INVALID_HANDLE && ht->lookup(dst_hid))
            ht->release(dst_hid);

        Result<HandleId> result = Result<HandleId>::err(Error::NoMem);

        if (dst_hid == K_INVALID_HANDLE) {
            result = ht->add(
                src->type,     src->resource, src->capabilities,
                src->transferable, src->destroy, src->acquire
            );
        } else {
            const VoidResult vr = ht->add_at(
                dst_hid,
                src->type,     src->resource, src->capabilities,
                src->transferable, src->destroy, src->acquire
            );
            result = vr.is_ok()
                ? Result<HandleId>::ok(dst_hid)
                : Result<HandleId>::err(vr.error());
        }

        if (result.is_err()) {
            if (src->acquire && src->resource)
                src->destroy(src->resource);
            return result.to_errno();
        }

        (void)flags;

        return static_cast<i64>(result.unwrap());
    }

    // sys_dup(old_hid) → new_hid
    i64 sys_dup(u64 arg0, u64, u64, u64, u64, u64) {
        return do_dup(arg0, K_INVALID_HANDLE, 0);
    }

    // sys_dup2(old_hid, new_hid) → new_hid
    i64 sys_dup2(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        return do_dup(
            arg0,
            arg1,
            0
        );
    }

    i64 sys_dup3(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        if (arg0 == arg1) return -EINVAL;
        return do_dup(
            arg0,
            arg1,
            0
        );
    }

}  // namespace syscalls::internal