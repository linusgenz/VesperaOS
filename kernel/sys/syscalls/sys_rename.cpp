// sys_rename.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 03.08.25.
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

#include <vespera/filesystem/vfs.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_rename(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto old_path = reinterpret_cast<const char*>(arg0);
        const auto new_path = reinterpret_cast<const char*>(arg1);

        char norm_old[256];
        if (!VFS::resolve_to_absolute(old_path, norm_old, sizeof(norm_old))) {
            return -EINVAL;
        }
        char norm_new[256];
        if (!VFS::resolve_to_absolute(new_path, norm_new, sizeof(norm_new))) {
            return -EINVAL;
        }

        SYSCALL_TRY_VOID(VFS::rename(norm_old, norm_new));
        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal