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
#include "../partition/partition.h"
#include <../devices/partition_device.h>
#include "vfs_helper.h"

size_t FilesystemDetector::driver_count = 0;
size_t FilesystemDetector::device_count = 0;

extern FileSystemDriver fat32_driver;
extern FileSystemDriver ext4_driver;
// TODO: Add other drivers when implemented
// extern FileSystemDriver ext4_driver;
// extern FileSystemDriver ntfs_driver;

Vector<PendingMount>* pending_mounts = nullptr;

void FilesystemDetector::Init() {
    device_count = 0;
    pending_mounts = new Vector<PendingMount>();

    Log::Info("[FS] Filesystem detector initialized");
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

    Log::Warning("[FS] No supported filesystem detected");
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

VfsNode *FilesystemDetector::MountFilesystem(BlockDevice *device, FilesystemInfo *fs_info) {
    if (!device || !fs_info) return nullptr;

    FileSystemDriver *driver = find_fs_driver(fs_info->type_name);
    if (!driver || !driver->mount) {
        Log::Error("[FS] No driver for %s", fs_info->type_name);
        return nullptr;
    }

    return driver->mount(device);
}

bool FilesystemDetector::mount_device(BlockDevice *device, const char *suggested_path, bool is_partition,
                                      size_t device_size) {
    FilesystemInfo fs_info{};
    if (!DetectFilesystem(device, &fs_info)) {
        Log::Warning("[FS] No supported filesystem detected on %s", suggested_path);
        return false;
    }

    VfsNode *root = MountFilesystem(device, &fs_info);
    if (!root) return false;

    if (strcmp(suggested_path, "/") != 0) {
        ensure_path_exists(suggested_path);
    }

    fs_info.mounted = true;

    DeviceDescriptor desc{};
    desc.device = device;
    desc.device_size = device_size;
    desc.is_recognized = true;
    desc.fs_info = fs_info;

    MountPoint mp{};
    strncpy(mp.path, suggested_path, sizeof(mp.path) - 1);
    mp.root = root;
    mp.device = new DeviceDescriptor(desc);
    mp.is_virtual = false;

    mount_points->push_back(mp);
    return true;
}


void FilesystemDetector::ScanAndMountAll() {
    auto devices = kernel::DeviceManager::GetDevices();
    size_t device_count_actual = kernel::DeviceManager::GetDeviceCount();

    if (device_count_actual == 0) {
        Log::Warning("[FS] No storage devices found");
        return;
    }

    int successful_mounts = 0;
    static bool root_assigned = false;

    for (size_t i = 0; i < device_count_actual; i++) {
        BlockDevice *device = devices[i];
        if (!device) continue;

        PartitionEntry parts[16];
        size_t pcount = parse_partitions(device, parts, 16);

        if (pcount == 0) {
            char mount_path[64];
            if (!root_assigned) {
                snprintf(mount_path, sizeof(mount_path), "/");
                if (mount_device(device, mount_path, false, 0)) {
                    root_assigned = true;
                    successful_mounts++;
                }
            } else {
                snprintf(mount_path, sizeof(mount_path), "/mnt/dev%d", i);
                if (root_assigned) {
                    ensure_path_exists(mount_path);
                    if (mount_device(device, mount_path, false, 0)) {
                        successful_mounts++;
                    }
                } else {
                    // vormerken
                    PendingMount pm{};
                    strncpy(pm.path, mount_path, sizeof(pm.path)-1);
                    pm.device = device;
                    pm.device_size = 0;
                    pm.is_partition = false;
                    pending_mounts->push_back(pm);
                }
            }
            continue;
        }

        for (size_t pi = 0; pi < pcount; ++pi) {
            PartitionEntry &pe = parts[pi];
            PartitionDevice *pdev = new PartitionDevice(device, pe.start_lba, pe.length_lba);

            // Heuristik: EFI-Partition?
            char mount_path[64];
            bool looks_like_esp = false;
            {
                FilesystemInfo fs_info{};
                if (DetectFilesystem(pdev, &fs_info)) {
                    VfsNode *probe_root = MountFilesystem(pdev, &fs_info);
                    if (probe_root && probe_root->ops && probe_root->ops->find) {
                        VfsNode *efi = probe_root->ops->find(probe_root, "EFI");
                        if (efi) {
                            VfsNode *boot = efi->ops ? efi->ops->find(efi, "BOOT") : nullptr;
                            if (boot) {
                                VfsNode *bootx64 = boot->ops ? boot->ops->find(boot, "BOOTX64.EFI") : nullptr;
                                if (bootx64) looks_like_esp = true;
                            }
                        }
                    }
                }
            }

            if (looks_like_esp) {
                snprintf(mount_path, sizeof(mount_path), "/efi");
            } else if (!root_assigned) {
                snprintf(mount_path, sizeof(mount_path), "/");
            } else {
                snprintf(mount_path, sizeof(mount_path), "/mnt/dev%dp%d", i, pi);
            }

            if (!root_assigned && strcmp(mount_path, "/") == 0) {
                // Root sofort mounten
                if (mount_device(pdev, mount_path, true, pe.length_lba)) {
                    root_assigned = true;
                    successful_mounts++;
                } else {
                    delete pdev;
                }
            } else if (root_assigned) {
                // Root existiert -> direkt mounten
                ensure_path_exists(mount_path);
                if (mount_device(pdev, mount_path, true, pe.length_lba)) {
                    successful_mounts++;
                } else {
                    delete pdev;
                }
            } else {
                // Root noch nicht existiert -> vormerken
                PendingMount pm{};
                strncpy(pm.path, mount_path, sizeof(pm.path)-1);
                pm.device = pdev;
                pm.device_size = pe.length_lba;
                pm.is_partition = true;
                pending_mounts->push_back(pm);
                Log::debug("[FS] Queued mount %s until root is ready", mount_path);
            }
        }
    }

    // Root gefunden? -> nachtragen
    if (root_assigned && pending_mounts->size() > 0) {
        for (auto& pm : *pending_mounts) {
            ensure_path_exists(pm.path);
            if (mount_device(pm.device, pm.path, pm.is_partition, pm.device_size)) {
                successful_mounts++;
            }
        }
        pending_mounts->clear();
    }

    if (successful_mounts == 0) {
        Log::Warning("[FS] No filesystems could be mounted automatically");
    }
}



void FilesystemDetector::PrintDetectedFilesystems() {
    Log::Info("[FS] === Detected Storage Devices ===");

    if (mount_points->empty()) {
        Log::Info("[FS] No devices detected");
        return;
    }

    int dev_index = 0;
    for (auto mp: (*mount_points)) {
        if (mp.is_virtual) continue;

        const DeviceDescriptor *dev = mp.device;
        if (!dev) continue;

        Log::Info("[FS] Device %d:", dev_index++);

        if (dev->is_recognized) {
            Log::Info("[FS]   Type: %s (%s)",
                      dev->fs_info.type_name,
                      dev->fs_info.description);

            if (dev->fs_info.mounted) {
                Log::Info("[FS]   Status: Mounted at %s", mp.path);
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
