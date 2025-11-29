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

#include <vector.h>

#include "fs_registry.h"
#include "../../kernel/devices/blockdevice.h"
#include "../../include/log.h"

struct PendingMount;

struct FilesystemInfo {
    const char *type_name;
    const char *description;
    bool mounted;
};

struct DeviceDescriptor {
    BlockDevice *device;
    size_t device_size;
    FilesystemInfo fs_info;
    bool is_recognized;
    const char *partition_table_type;
};

class FilesystemDetector {
public:
    static void Init();

    static void RegisterAllDrivers();

    static bool DetectFilesystem(BlockDevice *device, FilesystemInfo *info);

    static void ScanAndMountAll();

    static void PrintDetectedFilesystems();

private:
    static Vector<PendingMount> *pending_mounts;

    static size_t driver_count;
    static size_t device_count;

    static void GenerateMountPath(const char *fs_type, int index, char *out_path, size_t size);

    static VfsNode *MountFilesystem(BlockDevice *device, const FilesystemInfo *fs_info);

    static bool mount_device(BlockDevice *device, const char *suggested_path, bool is_partition, size_t device_size,
                             const char *table_type, bool is_root_device = false);

    static bool IsValidFilesystemType(const char *type);
};

namespace FilesystemProbes {
    bool ProbeFAT12(BlockDevice *device);

    bool ProbeFAT16(BlockDevice *device);

    bool ProbeFAT32(BlockDevice *device);

    bool ProbeEXT2(BlockDevice *device);

    bool ProbeEXT3(BlockDevice *device);

    bool ProbeEXT4(BlockDevice *device);

    bool ProbeNTFS(BlockDevice *device);

    bool ProbeISO9660(BlockDevice *device);
}

#endif //ENHANCED_FS_DETECTION_H
