// vfs_node.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef VFS_NODE_H
#define VFS_NODE_H
#include "stddef.h"

enum class VfsNodeType {
    File,
    Directory,
    Device
};

struct VfsNode;

struct VfsNodeOps {
    size_t (*read)(VfsNode* node, size_t offset, size_t size, void* buffer);
    size_t (*write)(VfsNode* node, size_t offset, size_t size, const void* buffer);
    VfsNode* (*find)(VfsNode* dir, const char* name);
    void (*close)(VfsNode* node);

    size_t (*file_size)(VfsNode*);
    int (*create)(VfsNode*, const char*);
    int (*rename)(VfsNode*, const char*, const char*);
   // int (*readdir)(VfsNode* dir, char* out_name, size_t max_len);
    int (*mkdir)(VfsNode*, const char*);
    int (*rmdir)(VfsNode*, const char*);
    int (*unlink)(VfsNode*, const char*);
};

struct VfsNode {
    const char* name;
    VfsNodeType type;
    void* internal_data;
    VfsNodeOps* ops;
    bool permanent;
};

#endif //VFS_NODE_H
