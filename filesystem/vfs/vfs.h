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

#include <vector.h>

#include "fs_detection.h"
#include "vfs_node.h"
#include "kernel/sync/spinlock.h"

struct FilesystemInfo;

struct MountPoint
{
    char path[64]{};
    VfsNode* root{};
    DeviceDescriptor* device{}; // null when virtual
    bool is_virtual = false;

    uint8_t is_root_device = false;
    uint8_t is_partition = false;

    MountPoint() = default;
    ~MountPoint() = default;

    MountPoint(const MountPoint&) = delete;
    MountPoint& operator=(const MountPoint&) = delete;
};

struct PendingMount
{
    char path[64];
    BlockDevice* device;
    size_t device_size;
    bool is_partition;
    const char* table_type;
};


struct VfsDir
{
    VfsNode* node;
    void* handle;
};


struct VfsStats
{
    size_t total_devices; // Total number of storage devices found
    size_t mounted_devices; // Number of successfully mounted devices
    size_t supported_filesystems; // Number of supported filesystem types
};


class VFS
{
public:
    static void init();

    static VfsNode* mount_virtual(VfsNode* root, const char* mount_path);

    static VfsNode* open(const char* path);

    static VfsDir* opendir(const char* path);

    static size_t read(const VfsNode* node, size_t offset, size_t size, void* buffer);

    static int readdir(const VfsDir* dir, dirent_t* out);

    static void close(VfsNode* node);

    static void closedir(VfsDir* dir);

    static int create(const char* path);

    static int rename(const char* oldPath, const char* newPath);

    static int mkdir(const char* path);

    static int rmdir(const char* path);

    static int unlink(const char* path);

    static bool probe_filesystem(BlockDevice* device);

    static void list_devices();

    static void remount_all();

    static void get_stats(VfsStats* stats);

    static void add_mount_point(MountPoint* mp);

    static size_t mount_points_count();

    static MountPoint* find_mount_point(const char* path);

    static bool remove_mount_point(MountPoint* mp);

    static Vector<MountPoint*> get_mount_points_snapshot()
    {
        spinlock_guard g(mount_points_lock);
        return mount_points->copy();
    }

    static bool resolve_parent(const char* path, VfsNode** parent_out, char* name_out);
    static dirent_type_t node_type_to_dirent_type(VfsNodeType type);
    static void ensure_path_exists(const char* path);

private:
    static spinlock_t mount_points_lock;
    static Vector<MountPoint*>* mount_points;
};

#endif //VFS_H
