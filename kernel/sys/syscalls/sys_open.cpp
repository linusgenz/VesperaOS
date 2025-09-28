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

#include <scheduling.h>

#include "../../../filesystem/vfs/vfs_node.h"
#include "../../../filesystem/vfs/vfs.h"
#include "../../include/errno.h"
#include "../../realm/realm_manager.h"
#include "../filesystem/vfs/vfs_handle.h"

namespace syscalls::internal {
int64_t sys_open(uint64_t arg0, uint64_t arg1, uint64_t, uint64_t, uint64_t, uint64_t) {
    const char *user_path = reinterpret_cast<const char *>(arg0);
    auto flags = static_cast<uint32_t>(arg1);

    if (!user_path || user_path[0] == '\0') return -EINVAL;

    Unit *current_unit = kernel::scheduling::get_current_unit();
    if (!current_unit) return -EINVAL;

    Realm *realm = RealmManager::get(current_unit->rid);
    if (!realm) return -EINVAL;

    VfsNode *node = vfs_open(user_path);
    if (!node) {
        return -ENOENT;
    }

    CapabilitySet required_caps = CAP_NONE;

    // Flags prüfen → READ/WRITE Rechte
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
            vfs_close(node);
            return -EINVAL;
    }

    VfsHandle *vh = nullptr;
    uint64_t handle_type = 0;

    // Node-Typ auswerten
    switch (node->type) {
        case VfsNodeType::Device:
            required_caps |= CAP_DEVICE_ACCESS;
            vh = new VfsHandle(node, flags, required_caps);
            if (!vh) {
                vfs_close(node);
                return -ENOMEM;
            }
            handle_type = HANDLE_TYPE_DEVICE;
            break;

        case VfsNodeType::File:
            vh = new VfsHandle(node, flags, required_caps);
            if (!vh) {
                vfs_close(node);
                return -ENOMEM;
            }
            handle_type = HANDLE_TYPE_FILE;
            break;

        case VfsNodeType::Directory: {
            if (!node->ops || !node->ops->opendir) {
                vfs_close(node);
                return -ENOTDIR;
            }
            void *dir_handle = node->ops->opendir(node);
            if (!dir_handle) {
                vfs_close(node);
                return -ENOMEM;
            }

            vh = new VfsHandle(node, flags, required_caps);
            if (!vh) {
                node->ops->closedir(dir_handle);
                vfs_close(node);
                return -ENOMEM;
            }

            vh->context->type_specific_data = dir_handle;
            handle_type = HANDLE_TYPE_DIRECTORY;
            break;
        }


        default:
            vfs_close(node);
            return -EINVAL;
    }

    // Capability-Check
    if ((realm->capabilities & required_caps) != required_caps) {
        delete vh;
        vfs_close(node);
        return -EACCES;
    }

    // Handle registrieren
    HandleID file_handle;
    ErrorCode err = realm->add_handle(
        handle_type,
        vh,
        required_caps,
        true,
        vfs_handle_destructor,
        &file_handle
    );

    if (err != MOD_SUCCESS) {
        if (node->type == VfsNodeType::Directory && vh->node->internal_data && node->ops && node->ops->closedir) {
            node->ops->closedir(vh->node->internal_data);
        }
        delete vh;
        return -err;
    }

    return file_handle;
}

}
