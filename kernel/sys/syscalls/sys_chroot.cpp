// sys_chroot.cpp
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

#include <filesystem/vfs.h>
#include <filesystem/vfs_node.h>
#include <klib/string.h>
#include <realm/realm.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>

namespace syscalls::internal {
    i64 sys_chroot(u64 arg0, u64, u64, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        if (!user_path || user_path[0] == '\0') return -EINVAL;

        const auto realm = kernel::scheduling::get_current_realm();

        char norm[256];
        SYSCALL_TRY_VOID(VFS::resolve_path(user_path, norm, sizeof(norm)));

        VfsNode* node = SYSCALL_TRY(VFS::open(norm));
        if (node->type != VfsNodeType::Directory) {
            VFS::close(node);
            return -ENOTDIR;
        }
        VFS::close(node);

        strncpy(realm->root_path, norm, sizeof(realm->root_path) - 1);
        realm->root_path[sizeof(realm->root_path) - 1] = '\0';

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal