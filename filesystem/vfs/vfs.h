// vfs.h
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

#ifndef VFS_H
#define VFS_H
#include "vfs_node.h"
#include "../../kernel/devices/blockdevice.h"
#include "../fat32/fat32.h"

struct MountPoint {
    char path[64];
    VfsNode* root;
};

struct VfsDir {
    VfsNode *node;
    size_t currentIndex;
    FAT32::FileEntry *entries;
    size_t entryCount;
};


void vfs_init();
int vfs_mount(BlockDevice* dev, const char* path, const char* fs_name);
VfsNode* vfs_open(const char* path);
VfsDir* vfs_opendir(const char *path);
size_t vfs_read(VfsNode* file, size_t offset, size_t size, void* buffer);
int vfs_readdir(VfsDir *dir, char *out_name, size_t max_len);
void vfs_close(VfsNode* node);
void vfs_closedir(VfsDir *dir);
int vfs_create(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);
int vfs_unlink(const char* path);

#endif //VFS_H
