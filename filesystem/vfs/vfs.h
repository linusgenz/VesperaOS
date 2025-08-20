// vfs.h
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

#ifndef VFS_H
#define VFS_H

#include "vfs_node.h"
#include "../../kernel/devices/blockdevice.h"
#include "../fat32/fat32.h"

struct FilesystemInfo;

struct MountPoint {
    char path[64];
    VfsNode* root;
};

struct VfsDir {
    VfsNode* node;
    void* handle;
};


struct VfsStats {
    size_t total_devices;           // Total number of storage devices found
    size_t mounted_devices;         // Number of successfully mounted devices
    size_t supported_filesystems;   // Number of supported filesystem types
};

void vfs_init();
VfsNode* vfs_mount(BlockDevice* device, const char* mount_path = nullptr); // Manual mount
bool vfs_probe(BlockDevice* device, FilesystemInfo* info);                 // Probe filesystem
void vfs_list_devices();                            // List all detected devices
bool vfs_supports(const char* fs_type);             // Check if filesystem type is supported
void vfs_remount_all();                             // Remount all devices (for testing)
void vfs_get_stats(VfsStats* stats);                // Get mount statistics


VfsNode* vfs_open(const char* path);
VfsDir* vfs_opendir(const char *path);
size_t vfs_read(VfsNode* file, size_t offset, size_t size, void* buffer);
int vfs_readdir(VfsDir *dir, char *out_name, size_t max_len);
size_t vfs_file_size(VfsNode* file);
void vfs_close(VfsNode* node);
void vfs_closedir(VfsDir *dir);
int vfs_create(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);
int vfs_unlink(const char* path);

#endif //VFS_H
