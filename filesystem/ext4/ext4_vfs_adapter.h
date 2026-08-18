// ext4_vfs_adapter.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.08.25.
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

#ifndef VESPERAOS_EXT4_VFS_ADAPTER_H
#define VESPERAOS_EXT4_VFS_ADAPTER_H
#include "ext4.h"

struct Ext4Node {
    ext4::FileSystem* fs;
    u32         inode;
    ext4::DirEntryType type;
    u64         file_size;
    char        path[512];

    ext4::FileEntry*  entries;
    usize       entry_count;
    usize       current_index;
};

struct Ext4DirHandle {
    ext4::FileEntry* entries;
    usize      count;
    usize      index;
};

#endif //VESPERAOS_EXT4_VFS_ADAPTER_H