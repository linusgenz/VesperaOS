// sys_chdir.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.03.26.
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
#include <klib/path.h>
#include <klib/string.h>
#include <vespera/scheduling.h>
#include <vespera/types.h>

namespace syscalls::internal {
    i64 sys_chdir(u64 arg0, u64, u64, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        if (!user_path || user_path[0] == '\0') return -EINVAL;

        const char* cwd = kernel::scheduling::get_current_cwd();

        char abs[256];
        if (user_path[0] != '/') {
            if (strcmp(cwd, "/") == 0)
                snprintf(abs, sizeof(abs), "/%s", user_path);
            else
                snprintf(abs, sizeof(abs), "%s/%s", cwd, user_path);
        } else {
            strncpy(abs, user_path, sizeof(abs) - 1);
            abs[sizeof(abs) - 1] = '\0';
        }

        char norm[256];
        normalize_path(abs, norm, sizeof(norm));

        VfsNode* node = SYSCALL_TRY(VFS::open(norm));

        if (node->type != VfsNodeType::Directory) {
            VFS::close(node);
            return -ENOTDIR;
        }
        VFS::close(node);

        if (!kernel::scheduling::set_current_cwd(norm)) return -EINVAL;

        return SUCCESS_CODE;
    }
}  // namespace syscalls::internal