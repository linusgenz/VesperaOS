// handle_resolution.cpp
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

#include "handle_resolution.h"

#include <realm/handle_table.h>
#include <uapi/vespera/handles.h>
#include <vespera/realm/realm.h>
#include <vespera/scheduling.h>

namespace syscalls {

    Result<ResolvedHandle> resolve_handle(const HandleId hid, const u64 type_mask, const capability_set required_caps) {
        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return Error::Srch;

        HandleEntry* he = realm->handle_table->lookup(hid);
        if (!he) return Error::BadH;

        if (required_caps != 0 && !(he->capabilities & required_caps)) return Error::Acces;

        if (type_mask != 0 && (he->type & HANDLE_TYPE_MASK) != type_mask)
            return Error::NotTty;  // POSIX convention: wrong fd type → ENOTTY

        return Result<ResolvedHandle>::ok({realm, he});
    }

}  // namespace syscalls
