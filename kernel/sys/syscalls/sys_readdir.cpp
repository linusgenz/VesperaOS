// sys_readdir.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 23.09.25.
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

#include <uapi/vespera/handles.h>
#include <vespera/types.h>

#include <filesystem/vfs/vfs_handle.h>
#include <kernel/units/unit.h>
#include "../handle_resolution.h"

namespace syscalls::internal {
    i64 sys_readdir(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const HandleId hid = arg0;
        auto* ent = reinterpret_cast<dirent_t*>(arg1);

        if (!ent) return -EINVAL;

        const auto rh = SYSCALL_TRY(resolve_handle(hid, HANDLE_TYPE_DIRECTORY, CAP_READ));

        const auto* vh = rh.resource_as<VfsHandle>();
        if (!vh || !vh->context || !vh->context->type_specific_data) return -EINVAL;

        const VfsDir* dir = vh->context->type_specific_data;

        const bool has_entry = SYSCALL_TRY(VFS::readdir(dir, ent));
        return has_entry ? 1 : 0;
    }
}  // namespace syscalls::internal
