// vfs_node.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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

#ifndef VFS_NODE_H
#define VFS_NODE_H
#include <stddef.h>
#include <stdint.h>
#include <uapi/vespera/dirent.h>

#include "../../include/vespera/types.h"

enum class VfsNodeType {
    File,
    Directory,
    CharDevice,
    BlockDevice,
    OtherDevice,
};

struct VfsNode;

struct VfsNodeOps {
    ssize_t (*read)(const VfsNode *node, size_t offset, size_t size, void *buffer);

    ssize_t (*write)(VfsNode *node, size_t offset, size_t size, const void *buffer);

    VfsNode * (*find)(const VfsNode *dir, const char *name);

    void (*close)(VfsNode *node);

    void * (*opendir)(const VfsNode *dir);

    int (*readdir)(void *dir_handle, dirent_t *out_name);

    void (*closedir)(void *dir_handle);

    int (*create)(const VfsNode *node, const char *name);

    int (*rename)(const VfsNode *, const char *old_name, const char *new_name);

    int (*mkdir)(const VfsNode *node, const char *name);

    int (*rmdir)(const VfsNode *node, const char *name);

    int (*unlink)(const VfsNode *node, const char *name);

    ssize_t (*ioctl)(const VfsNode *node, uint32_t cmd, void *arg);
};

struct VfsNode {
    const char *name;
    size_t size; // size of the file is equal to fileSize field in internal_data
    VfsNodeType type;
    void *internal_data;
    VfsNodeOps *ops;
    bool permanent;
};

#endif //VFS_NODE_H
