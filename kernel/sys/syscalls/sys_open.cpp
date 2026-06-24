// sys_open.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.08.25.
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
#include <uapi/vespera/fflags.h>
#include <uapi/vespera/handles.h>
#include <vespera/realm/handles.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <klib/string.h>
#include "filesystem/vfs_handle.h"

namespace syscalls::internal {
    i64 sys_open(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        const auto flags = static_cast<u32>(arg1);

        if (!user_path || user_path[0] == '\0') return -EINVAL;

        const Unit* current_unit = kernel::scheduling::get_current_unit();
        if (!current_unit) return -EINVAL;

        Realm* realm = kernel::scheduling::get_current_realm();
        if (!realm) return -ESRCH;

        if (strcmp("/etc/init.lua", user_path) != 0) {

        }

        char norm[256];
        SYSCALL_TRY_VOID(VFS::resolve_path(user_path, norm, sizeof(norm)));

        auto node_res = VFS::open(norm);

        if (node_res.is_err()) {
            if (flags & O_CREAT) {
                SYSCALL_TRY_VOID(VFS::create(norm));
                node_res = VFS::open(norm);
                if (node_res.is_err()) return -ENOENT;
            } else {
                return node_res.to_errno();
            }
        } else {
            if ((flags & O_CREAT) && (flags & O_EXCL)) {
                VFS::close(node_res.unwrap());
                return -EEXIST;
            }
        }

        VfsNode* node = node_res.unwrap();

        u32 vfs_access = 0;
        capability_set required_caps = CAP_NONE;

        switch (flags & 0x3) {
            case O_RDONLY:
                vfs_access = kernel::security::VFS_ACCESS_READ;
                required_caps = CAP_READ;
                break;
            case O_WRONLY:
                vfs_access = kernel::security::VFS_ACCESS_WRITE;
                required_caps = CAP_WRITE;
                break;
            case O_RDWR:
                vfs_access = kernel::security::VFS_ACCESS_READ | kernel::security::VFS_ACCESS_WRITE;
                required_caps = CAP_READ | CAP_WRITE;
                break;
            default:
                VFS::close(node);
                return -EINVAL;
        }

        if (const int err = kernel::security::vfs_check_permission(
                node, vfs_access, SYSCALL_TRY(kernel::security::current_credentials())
            );
            err != 0) {
            VFS::close(node);
            return err;
        }

        if (node->type == VfsNodeType::Directory) {
            if (!(flags & O_DIRECTORY)) {
                VFS::close(node);
                return -EISDIR;
            }
        } else {
            if (flags & O_DIRECTORY) {
                VFS::close(node);
                return -ENOTDIR;
            }
        }

        VfsHandle* vh = nullptr;
        u64 handle_type = 0;

        switch (node->type) {
            case VfsNodeType::CharDevice:
            case VfsNodeType::BlockDevice:
                required_caps |= CAP_DEVICE_ACCESS;
                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    VFS::close(node);
                    return -ENOMEM;
                }
                handle_type = HANDLE_TYPE_DEVICE;
                break;

            case VfsNodeType::File:
                if (flags & O_TRUNC) {
                    auto trunc_res = VFS::truncate(node, 0);
                    if (trunc_res.is_err()) {
                        VFS::close(node);
                        return trunc_res.to_errno();
                    }
                }
                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    VFS::close(node);
                    return -ENOMEM;
                }
                handle_type = HANDLE_TYPE_FILE;
                break;

            case VfsNodeType::Directory: {
                auto dir_res = VFS::opendir(node);
                if (dir_res.is_err()) {
                    VFS::close(node);
                    return dir_res.to_errno();
                }
                VfsDir* dir_handle = dir_res.unwrap();

                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    VFS::closedir(dir_handle);
                    VFS::close(node);
                    return -ENOMEM;
                }
                vh->context->type_specific_data = dir_handle;
                handle_type = HANDLE_TYPE_DIRECTORY;
                break;
            }

            case VfsNodeType::OtherDevice:
                required_caps |= CAP_DEVICE_ACCESS;
                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    VFS::close(node);
                    return -ENOMEM;
                }
                handle_type = HANDLE_TYPE_DEVICE;
                break;

            default:
                VFS::close(node);
                return -EINVAL;
        }

        if (const capability_set caps = kernel::scheduling::get_current_capabilities();
            (caps & required_caps) != required_caps) {
            delete vh;
            VFS::close(node);
            return -EACCES;
        }

        if ((flags & O_APPEND) && node->type == VfsNodeType::File) vh->context->position = node->size;

        const Result<HandleId> result =
            kernel::realm::add_handle_to_current(handle_type, vh, required_caps, true, vfs_handle_destructor, nullptr);

        if (result.is_err()) {
            if (node->type == VfsNodeType::Directory && vh->node->internal_data && node->ops && node->ops->closedir)
                VFS::closedir(static_cast<VfsDir*>(vh->node->internal_data));
            delete vh;
            return result.to_errno();
        }

        return static_cast<i64>(result.unwrap());
    }
}  // namespace syscalls::internal
