// vfs_handle.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.09.25.
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

#ifndef VESPERAOS_VFS_HANDLE_H
#define VESPERAOS_VFS_HANDLE_H

#include <cstdint>

#include "../devfs/devfs.h"
#include "vfs.h"

struct VfsHandleContext {
    uint32_t open_flags; // O_RDONLY, O_WRONLY, O_RDWR
    size_t position; // used for offset
    CapabilitySet required_caps;
    void *type_specific_data;
};

struct VfsHandle {
    VfsNode *node;
    VfsHandleContext *context;

    VfsHandle(VfsNode *n, uint32_t flags, CapabilitySet caps) : node(n) {
        context = new VfsHandleContext();
        context->open_flags = flags;
        context->position = 0;
        context->required_caps = caps;
        context->type_specific_data = nullptr;
    }

    ~VfsHandle() {
        if (node) {
            if (node->type == VfsNodeType::Directory &&
                context && context->type_specific_data &&
                node->ops && node->ops->closedir) {
                node->ops->closedir(context->type_specific_data);
                context->type_specific_data = nullptr;
            }

            VFS::close(node);
        }
        delete context;
    }
};

static void vfs_handle_destructor(void *resource) {
    const auto *vh = static_cast<VfsHandle *>(resource);
    delete vh;
}

#endif //VESPERAOS_VFS_HANDLE_H
