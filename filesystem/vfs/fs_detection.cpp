// enhanced_fs_detection.cpp
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

#include "fs_detection.h"
#include "../../include/string.h"
#include <memory.h>
#include "../../kernel/devices/device_manager.h"
#include "../fat32/fat32_vfs_adapter.h"
#include "vfs.h"

size_t FilesystemDetector::driver_count = 0;
DeviceDescriptor FilesystemDetector::detected_devices[MAX_MOUNTS];
size_t FilesystemDetector::device_count = 0;

extern FileSystemDriver fat32_driver;
extern FileSystemDriver ext4_driver;
// TODO: Add other drivers when implemented
// extern FileSystemDriver ext4_driver;
// extern FileSystemDriver ntfs_driver;

void FilesystemDetector::Init() {
    device_count = 0;

    for (int i = 0; i < MAX_MOUNTS; i++) {
        detected_devices[i] = {};
    }

  //  Log::Info("[FS] Filesystem detector initialized");
}

void FilesystemDetector::RegisterAllDrivers() {
    // FAT32 driver
    if (fs_driver_count() < MAX_FS_DRIVERS) {
        register_fs_driver(&fat32_driver);
   //     Log::Info("[FS] Registered FAT32 driver");
    }

    // EXT4 driver
    if (fs_driver_count() < MAX_FS_DRIVERS) {
        register_fs_driver(&ext4_driver);
     //   Log::Info("[FS] Registered EXT4 driver");
    }

 //   Log::Info("[FS] Registered %d filesystem drivers", fs_driver_count());
}

bool FilesystemDetector::DetectFilesystem(BlockDevice *device, FilesystemInfo *info) {
    if (!device || !info) return false;

    // Initialize info structure
    info->type_name = nullptr;
    info->description = nullptr;
    info->mounted = false;
    info->mount_point = nullptr;
    info->mount_path[0] = '\0';

    for (size_t i = 0; i < fs_driver_count(); i++) {
        FileSystemDriver *drv = fs_driver_at(i);
        if (drv && drv->probe && drv->probe(device)) {
            info->type_name = drv->name;

            if (strcmp(drv->name, "fat32") == 0) {
                info->description = "Microsoft FAT32 Filesystem";
            } else if (strcmp(drv->name, "ext4") == 0) {
                info->description = "Linux Extended Filesystem v4";
            } else if (strcmp(drv->name, "ntfs") == 0) {
                info->description = "Microsoft NTFS Filesystem";
            } else {
                info->description = "Unknown Filesystem";
            }

         //   Log::Info("[FS] Detected filesystem: %s (%s)", info->type_name, info->description);
            return true;
        }
    }

    Log::Warning("[FS] No supported filesystem detected on device");
    return false;
}

void FilesystemDetector::GenerateMountPath(const char *fs_type, int index, char *out_path, size_t size) {
    if (strcmp(fs_type, "fat32") == 0) {
        snprintf(out_path, size, "/mnt/fat32_%d", index);
    } else if (strcmp(fs_type, "ext4") == 0) {
        snprintf(out_path, size, "/mnt/ext4_%d", index);
    } else if (strcmp(fs_type, "ntfs") == 0) {
        snprintf(out_path, size, "/mnt/ntfs_%d", index);
    } else {
        snprintf(out_path, size, "/mnt/disk%d", index);
    }
}

VfsNode *FilesystemDetector::TryMount(BlockDevice *device, const char *mount_path) {
    if (!device) return nullptr;

    FilesystemInfo fs_info;
    if (!DetectFilesystem(device, &fs_info)) {
        Log::Warning("[FS] Cannot mount: No supported filesystem detected");
        return nullptr;
    }

    FileSystemDriver *driver = find_fs_driver(fs_info.type_name);
    if (!driver || !driver->mount) {
        Log::Error("[FS] No driver available to mount filesystem: %s",
                   fs_info.type_name ? fs_info.type_name : "<unknown>");
        return nullptr;
    }

    // Try to mount
    VfsNode *root = driver->mount(device);
    if (!root) {
        Log::Error("[FS] Failed to mount %s filesystem", fs_info.type_name);
        return nullptr;
    }

    // Generate mount path if not provided
    char *final_mount_path;
    static char generated_path[64];

    if (mount_path) {
        final_mount_path = (char *) mount_path;
    } else {
        static int mount_index = 0;
        snprintf(generated_path, sizeof(generated_path), "/mnt/%s_%d",
                 fs_info.type_name, mount_index++);
        final_mount_path = generated_path;
    }

    Log::Info("[FS] Successfully mounted %s at %s", fs_info.type_name, final_mount_path);
    return root;
}

void FilesystemDetector::ScanAndMountAll() {
  //  Log::Info("[FS] Starting automatic filesystem detection and mounting...");

    auto devices = kernel::DeviceManager::GetDevices();
    size_t device_count_actual = kernel::DeviceManager::GetDeviceCount();

    if (device_count_actual == 0) {
        Log::Warning("[FS] No storage devices found");
        return;
    }

    Log::Info("[FS] Found %d storage devices", device_count_actual);

    int successful_mounts = 0;

    for (size_t i = 0; i < device_count_actual && i < MAX_MOUNTS; i++) {
        BlockDevice *device = devices[i];
        if (!device) continue;

       // Log::Info("[FS] Processing device %d", i);

        // Store device information
        detected_devices[device_count].device = device;
        detected_devices[device_count].device_size = 0; // TODO: Get actual device size
        detected_devices[device_count].is_recognized = false;

        // Try to detect and mount
        if (DetectFilesystem(device, &detected_devices[device_count].fs_info)) {
            detected_devices[device_count].is_recognized = true;

            if (VfsNode *mount_point = TryMount(device)) {
                GenerateMountPath(detected_devices[device_count].fs_info.type_name, i,
                                  detected_devices[device_count].fs_info.mount_path,
                                  sizeof(detected_devices[device_count].fs_info.mount_path));

                detected_devices[device_count].fs_info.mounted = true;
                detected_devices[device_count].fs_info.mount_point = mount_point;
                successful_mounts++;
            } else {
                Log::Warning("[FS] Failed to mount detected %s filesystem",
                             detected_devices[device_count].fs_info.type_name);
            }
        } else {
            Log::Info("[FS] Device %d: Unknown or unsupported filesystem", i);
        }

        device_count++;
    }

  //  Log::Info("[FS] Filesystem detection complete: %d/%d devices mounted successfully",
   //           successful_mounts, device_count_actual);

    if (successful_mounts == 0) {
        Log::Warning("[FS] No filesystems could be mounted automatically");
    }
}

void FilesystemDetector::PrintDetectedFilesystems() {
    Log::Info("[FS] === Detected Storage Devices ===");

    if (device_count == 0) {
        Log::Info("[FS] No devices detected");
        return;
    }

    for (size_t i = 0; i < device_count; i++) {
        const auto &dev = detected_devices[i];

        Log::Info("[FS] Device %d:", i);

        if (dev.is_recognized) {
            Log::Info("[FS]   Type: %s (%s)",
                      dev.fs_info.type_name,
                      dev.fs_info.description);

            if (dev.fs_info.mounted) {
                Log::Info("[FS]   Status: Mounted at %s", dev.fs_info.mount_path);
            } else {
                Log::Info("[FS]   Status: Detected but not mounted");
            }
        } else {
            Log::Info("[FS]   Type: Unknown/Unsupported");
            Log::Info("[FS]   Status: Not mounted");
        }
    }

    Log::Info("[FS] === End of Device List ===");
}

bool FilesystemDetector::IsValidFilesystemType(const char *type) {
    if (!type) return false;
    return find_fs_driver(type) != nullptr;
}

// Filesystem probe implementations
namespace FilesystemProbes {
    bool ProbeFAT12(BlockDevice *device) {
        // TODO: Implement FAT12-specific detection
        return false;
    }

    bool ProbeFAT16(BlockDevice *device) {
        // TODO: Implement FAT16-specific detection
        return false;
    }

    bool ProbeFAT32(BlockDevice *device) {
        // Use existing FAT32 detection from fat32_driver
        return fat32_driver.probe(device);
    }

    bool ProbeEXT2(BlockDevice *device) {
        // TODO: Implement EXT2 detection
        // Check for EXT2 magic number (0xEF53) at offset 1080
        uint8_t buffer[512];
        if (!device->read(2, 1, buffer)) {
            return false;
        }

        // EXT2/3/4 magic number at offset 56 within the superblock
        uint16_t *magic = (uint16_t *) (buffer + 56);
        return (*magic == 0xEF53);
    }

    bool ProbeEXT3(BlockDevice *device) {
        // EXT3 is EXT2 with journaling, same magic number
        // TODO: Add journal detection
        return ProbeEXT2(device);
    }

    bool ProbeEXT4(BlockDevice *device) {
        // TODO: Implement proper EXT4 detection
        return ProbeEXT2(device);
    }

    bool ProbeNTFS(BlockDevice *device) {
        // TODO: Implement NTFS detection
        // Check for NTFS magic "NTFS    " at offset 3
        uint8_t buffer[512];
        if (!device->read(0, 1, buffer)) {
            return false;
        }

        return (memcmp(buffer + 3, "NTFS    ", 8) == 0);
    }

    bool ProbeISO9660(BlockDevice *device) {
        // TODO: Implement ISO9660 detection
        // Check for ISO9660 magic at sector 16
        uint8_t buffer[512];
        if (!device->read(16, 1, buffer)) {
            return false;
        }

        return (memcmp(buffer + 1, "CD001", 5) == 0);
    }
}