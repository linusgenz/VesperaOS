// sys_mknod.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.08.26.
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
#include <security/permission.h>
#include <uapi/vespera/fcntl.h>
#include <uapi/vespera/handles.h>
#include <vespera/realm/handles.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <klib/string.h>
#include "filesystem/vfs_handle.h"

// NOTE: S_IFMT/S_IFIFO/S_IFREG/S_IFDIR/S_IFCHR/S_IFBLK are assumed to live in
// uapi/vespera/stat.h with standard POSIX values (S_IFIFO == 0010000). Adjust
// the include/values below if your actual header differs.
#include <uapi/vespera/stat.h>

#include "sys/handle_resolution.h"

namespace syscalls::internal {
    namespace {
        i64 mknod_impl(const char* user_path, mode_t mode, u64 dev, const char* base_path) {
            if (!user_path || user_path[0] == '\0') return -EINVAL;

            const Unit* current_unit = kernel::scheduling::get_current_unit();
            if (!current_unit) return -EINVAL;

            Realm* realm = kernel::scheduling::get_current_realm();
            if (!realm) return -ESRCH;

            char norm[256];
            if (base_path != nullptr) {
                SYSCALL_TRY_VOID(VFS::resolve_path_at(base_path, user_path, norm, sizeof(norm)));
            } else {
                SYSCALL_TRY_VOID(VFS::resolve_path(user_path, norm, sizeof(norm)));
            }


            u32 node_type = mode & S_IFMT;
            const mode_t perm_bits = mode & 07777;

            switch (node_type) {
                case S_IFIFO:
                    (void)dev;
                    break;

                case 0: // mode with no type bits set is treated as a regular file, like POSIX mknod()
                    node_type = S_IFREG;
                    break;

                case S_IFCHR:
                case S_IFBLK:
                    // TODO add this when we use ext4 for devices.
                    return -ENOSYS;

                case S_IFDIR:
                default:
                    return -EINVAL;
            }

            SYSCALL_TRY_VOID(VFS::create(norm, perm_bits | node_type));

            return 0;
        }
    }  // namespace

    i64 sys_mknod(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        const auto mode = static_cast<mode_t>(arg1);
        const auto dev = arg2;

        return mknod_impl(user_path, mode, dev, nullptr);
    }

    i64 sys_mknodat(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        const HandleId dirhid = arg0;
        const auto user_path = reinterpret_cast<const char*>(arg1);
        const auto mode = static_cast<mode_t>(arg2);
        const auto dev = arg3;

        if (dirhid == static_cast<u64>(AT_FDCWD)) return mknod_impl(user_path, mode, dev, nullptr);
        if (user_path && user_path[0] == '/') return mknod_impl(user_path, mode, dev, nullptr);

        const auto rh = SYSCALL_TRY(resolve_handle(dirhid, HANDLE_TYPE_DIRECTORY, CAP_WRITE));

        const auto* dir_handle = rh.resource_as<VfsHandle>();
        if (!dir_handle || !dir_handle->node || !dir_handle->context) return -EBADH;
        if (dir_handle->node->type != VfsNodeType::Directory) return -ENOTDIR;
        if (dir_handle->context->path[0] == '\0') return -EBADH;

        return mknod_impl(user_path, mode, dev, dir_handle->context->path);
    }
}  // namespace syscalls::internal