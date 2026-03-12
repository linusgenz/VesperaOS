// fs_registry.h
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

#ifndef FS_REGISTRY_H
#define FS_REGISTRY_H

#include <vespera/devices/block.h>
#include "fs_detection.h"
#include "vfs_node.h"

#define MAX_MOUNTS 8
#define MAX_FS_DRIVERS MAX_MOUNTS

struct FileSystemDriver {
    const char* name;
    int (*probe)(BlockDevice* dev, FilesystemInfo *fs_info); // 1 = valid
    VfsNode* (*mount)(BlockDevice* dev);
    bool (*unmount)(VfsNode* root);
    bool (*force_unmount)(VfsNode* root);
};

FileSystemDriver* fs_driver_at(usize i);
usize fs_driver_count();
void register_fs_driver(FileSystemDriver* driver);
FileSystemDriver* find_fs_driver(const char* name);
VfsNode* try_mount(BlockDevice* dev);


#endif //FS_REGISTRY_H
