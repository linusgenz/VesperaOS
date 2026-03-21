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

#include <uapi/vespera/fflags.h>
#include <uapi/vespera/handels.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "../../../filesystem/vfs/vfs.h"
#include "../../../filesystem/vfs/vfs_node.h"
#include "../filesystem/vfs/vfs_handle.h"

namespace syscalls::internal {
    i64 sys_open(u64 arg0, u64 arg1, u64, u64, u64, u64) {
        const auto user_path = reinterpret_cast<const char*>(arg0);
        const auto flags = static_cast<u32>(arg1);

        if (!user_path || user_path[0] == '\0') return -EINVAL;

        const Unit* current_unit = kernel::scheduling::get_current_unit();
        if (!current_unit) return -EINVAL;

        Realm* realm = RealmManager::get(current_unit->rid);
        if (!realm) return -EINVAL;

        char norm[256];
        if (!VFS::resolve_to_absolute(user_path, norm, sizeof(norm))) {
            return -EINVAL;
        }

        VfsNode* node = VFS::open(norm);

        if (!node) {
            if (flags & O_CREAT) {
                if (const int result = VFS::create(norm); result != 0) {
                    return result;
                }

                node = VFS::open(norm);
                if (!node) {
                    return -ENOENT;
                }
            } else {
                return -ENOENT;
            }
        } else {
            if ((flags & O_CREAT) && (flags & O_EXCL)) {
                VFS::close(node);
                return -EEXIST;
            }
        }

        capability_set required_caps = CAP_NONE;

        switch (flags & 0x3) {
            case O_RDONLY:
                required_caps |= CAP_READ;
                break;
            case O_WRONLY:
                required_caps |= CAP_WRITE;
                break;
            case O_RDWR:
                required_caps |= CAP_READ | CAP_WRITE;
                break;
            default:
                VFS::close(node);
                return -EINVAL;
        }

        if (node->type == VfsNodeType::Directory) {
            // User did not want a directory → EISDIR
            if (!(flags & O_DIRECTORY)) {
                VFS::close(node);
                return -EISDIR;
            }
        } else {
            // User WANTS a directory, but the target is not one
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
                    if (node->ops && node->ops->truncate) {
                        if (const int r = VFS::truncate(node, 0); r < 0) {
                            VFS::close(node);
                            return r;
                        }
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
                if (!node->ops || !node->ops->opendir) {
                    VFS::close(node);
                    return -ENOTDIR;
                }
                void* dir_handle = node->ops->opendir(node);
                if (!dir_handle) {
                    VFS::close(node);
                    return -ENOMEM;
                }

                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    node->ops->closedir(dir_handle);
                    VFS::close(node);
                    return -ENOMEM;
                }

                vh->context->type_specific_data = dir_handle;
                handle_type = HANDLE_TYPE_DIRECTORY;
                break;
            }
            case VfsNodeType::OtherDevice: {
                required_caps |= CAP_DEVICE_ACCESS;
                vh = new VfsHandle(node, flags, required_caps);
                if (!vh) {
                    VFS::close(node);
                    return -ENOMEM;
                }
                handle_type = HANDLE_TYPE_DEVICE;
                break;
            }

            default:
                VFS::close(node);
                return -EINVAL;
        }

        // Capability-Check
        if ((realm->capabilities & required_caps) != required_caps) {
            delete vh;
            VFS::close(node);
            return -EACCES;
        }

        if (flags & O_APPEND) {
            if (node->type == VfsNodeType::File) {
                vh->context->position = node->size;
            }
        }

        // Handle registrieren
        HandleId file_handle = 0;

        if (const i64 err =
                realm->add_handle(handle_type, vh, required_caps, true, vfs_handle_destructor, nullptr, &file_handle);
            err != SUCCESS_CODE) {
            if (node->type == VfsNodeType::Directory && vh->node->internal_data && node->ops && node->ops->closedir) {
                node->ops->closedir(vh->node->internal_data);
            }
            delete vh;
            return -err;
        }

        return file_handle;
    }
}  // namespace syscalls::internal
