// fs_detection.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 19.08.25.
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

#ifndef ENHANCED_FS_DETECTION_H
#define ENHANCED_FS_DETECTION_H

#include <klib/vector.h>
#include <vespera/devices/block.h>
#include <vespera/devices/kernel_device.h>

#include "vfs_node.h"

struct MountPoint;
struct PendingMount;

struct FilesystemInfo {
    const char *type_name;
    const char *description;
    bool mounted;
    char label[16];
};

struct BlkDeviceDescriptor {
    BlockDevice *device;
    u8 device_id;
    usize device_size;
    FilesystemInfo fs_info;
    bool is_recognized;
    const char *partition_table_type;
};

class FilesystemDetector {
   public:
    static void init();

    static void register_all_drivers();

    static bool detect_filesystem(BlkDeviceDescriptor* blk_desc);

    static void scan_and_mount_all();

    static void unmount_all();

    static bool unmount(MountPoint *mp);

    static void emergency_detach_device(const BlockDevice *device);

    static void print_detected_filesystems();

    static i64 mount_manual(const KernelDevice *device, const char *target, const char *fstype, u64 flags);

   private:
    static usize driver_count_;
    static usize device_count_;

    static VfsNode *mount_filesystem(const BlkDeviceDescriptor * blk_desc);

    static bool mount_device(
        BlkDeviceDescriptor *blk_desc, const char *suggested_path, bool is_partition, bool is_root_device
    );
};

#endif  // ENHANCED_FS_DETECTION_H
