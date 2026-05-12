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

#include <klib/vector.h>
#include <filesystem/vfs/fs_detection.h>
#include <filesystem/vfs/vfs_node.h>
#include "vespera/sync/spinlock.h"

struct FilesystemInfo;

struct MountPoint
{
    char path[64]{};
    VfsNode* root = nullptr;
    BlkDeviceDescriptor* device{}; // null when virtual
    bool is_virtual = false;

    bool is_root_device = false;
    bool is_partition = false;

    u64 flags = 0;

    MountPoint() = default;
    ~MountPoint() = default;

    MountPoint(const MountPoint&) = delete;
    MountPoint& operator=(const MountPoint&) = delete;
};

struct PendingMount
{
    char path[64];
    BlkDeviceDescriptor desc;
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
    usize total_devices; // Total number of storage devices found
    usize mounted_devices; // Number of successfully mounted devices
    usize supported_filesystems; // Number of supported filesystem types
};


class VFS
{
public:
    static void init();

    static VfsNode* mount_virtual(VfsNode* root, const char* mount_path);

    static Result<VfsNode*> open(const char* path);

    static Result<VfsDir*> opendir(VfsNode* node);

    static Result<usize> read(const VfsNode* node, usize offset, usize size, void* buffer);
    static Result<usize> write(VfsNode* node, usize offset, usize size, const void* buffer);

    static Result<bool> readdir(const VfsDir* dir, dirent_t* out);

    static void close(VfsNode* node);

    static void closedir(VfsDir* dir);

    static VoidResult create(const char* path);

    static VoidResult rename(const char* old_path, const char* new_path);

    static VoidResult mkdir(const char* path);

    static VoidResult rmdir(const char* path);

    static VoidResult unlink(const char* path);
    static VoidResult truncate(VfsNode* node, usize new_size);
    static VoidResult stat(const VfsNode* node, vespera_stat_t* out);
    static VoidResult chown(VfsNode* node, u32 uid, u32 gid);
    static VoidResult chmod(VfsNode* node, u16 mode);

    // static bool probe_filesystem(BlockDevice* device);

    static void list_devices();

    static void remount_all();

    //static void get_stats(VfsStats* stats);

    static void add_mount_point(MountPoint* mp);

    static usize mount_points_count();

    static MountPoint* find_mount_point(const char* path);

    static bool remove_mount_point(const MountPoint* mp);

    static Vector<MountPoint*> get_mount_points_snapshot()
    {
        SpinlockGuard g(mount_points_lock_);
        return mount_points_->copy();
    }

    static bool resolve_parent(const char* path, VfsNode** parent_out, char* name_out);
    static dirent_type_t node_type_to_dirent_type(VfsNodeType type);
    static void ensure_path_exists(const char* path);
    static bool resolve_to_absolute(const char* user_path, char* out, usize out_size);

   private:
    static Spinlock mount_points_lock_;
    static Vector<MountPoint*>* mount_points_;
};

#endif //VFS_H
