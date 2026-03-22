// fat32_vfs_adapter.h
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

#ifndef FAT32_VFS_ADAPTER_H
#define FAT32_VFS_ADAPTER_H

#include <vespera/filesystem/vfs.h>
#include "fat32.h"

    struct Fat32Node {
        fat32::FileSystem *fs;
        char path[256];
        u32 parent_cluster;
        u32 cluster;
        bool is_dir;
        usize file_size;
        usize entry_count;
        usize first_lfn_index;
        usize current_index;
        fat32::DirectoryEntry dir_entry;
    };

    struct Fat32DirHandle {
        fat32::FileEntry* entries;
        usize count;
        usize index;
    };

    VfsNode *wrap_fat32_root(fat32::FileSystem * fs);

    int fat32_probe(BlockDevice *dev);

    VfsNode *fat32_mount(BlockDevice *dev,  char* out, int out_size);

#endif //FAT32_VFS_ADAPTER_H
