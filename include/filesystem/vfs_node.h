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

#include <uapi/vespera/dirent.h>
#include <vespera/types.h>

#include "klib/result.h"
#include "uapi/vespera/stat.h"

struct MountPoint;
enum class VfsNodeType : u8 {
    File,
    Directory,
    CharDevice,
    BlockDevice,
    OtherDevice,
};

struct VfsNode;

struct VfsNodeOps {
    Result<usize> (*read)(const VfsNode *node, usize offset, usize size, void *buffer);

    Result<usize> (*write)(VfsNode *node, usize offset, usize size, const void *buffer);

    Result<VfsNode *> (*find)(VfsNode *dir, const char *name);

    void (*close)(VfsNode *node);

    Result<void *> (*opendir)(const VfsNode *dir);

    Result<bool> (*readdir)(void *dir_handle, dirent_t *out_name);

    void (*closedir)(void *dir_handle);

    VoidResult (*create)(const VfsNode *node, const char *name, mode_t mode);

    VoidResult (*rename)(const VfsNode *, const char *old_name, const VfsNode *new_parent, const char *new_name);

    VoidResult (*mkdir)(const VfsNode *node, const char *name, mode_t mode);

    VoidResult (*rmdir)(const VfsNode *node, const char *name);

    VoidResult (*unlink)(const VfsNode *node, const char *name);

    isize (*ioctl)(const VfsNode *node, u32 cmd, void *arg);

    VoidResult (*stat)(const VfsNode *, stat *out);

    VoidResult (*truncate)(VfsNode *node, usize new_size);

    VoidResult (*chown)(VfsNode *node, u32 uid, u32 gid);

    VoidResult (*chmod)(VfsNode *node, u16 mode);

    int (*poll)(const VfsNode *node);
};

struct VfsNode {
    const char *name;
    usize size;  // size of the file is equal to fileSize field in internal_data
    const MountPoint *mount = nullptr;
    void *internal_data;
    const VfsNodeOps *ops;
    VfsNodeType type;
    bool permanent;
    bool seekable = false;
    usize ref_count = 1;

    u64 inode_id = 0;

};

VfsNode* ref_node(VfsNode* node);
void unref_node(VfsNode* node);

#endif  // VFS_NODE_H
