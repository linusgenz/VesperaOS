// handle_transfer.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.09.26.
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

#include <klib/result.h>

#include "handle_table.h"
#include "vespera/realm/handles.h"

namespace kernel::realm {
    Result<HandleId> transfer_handle_to_realm(
        const HandleEntry* src_entry,
        Realm* dst,
        const capability_set caps_mask
    ) {
        if (!src_entry->transferable) return Error::Acces;

        const capability_set granted = src_entry->capabilities & caps_mask;
        if (granted == 0) return Error::Acces;

        if (!src_entry->acquire || !src_entry->resource) return Error::Inval;
        src_entry->acquire(src_entry->resource); // bump refcount on the shared resource

        const Result<HandleId> result = kernel::realm::add_handle(
            dst,
            src_entry->type,
            src_entry->resource,
            granted,
            /*transferable=*/true,
            src_entry->destroy,
            src_entry->acquire
        );

        if (result.is_err()) {
            src_entry->destroy(src_entry->resource); // undo the acquire() above
            return result.error();
        }

        return result;
    }
}