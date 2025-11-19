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
#include "../../kernel/system/system_manager.h"

size_t FilesystemDetector::driver_count = 0;
size_t FilesystemDetector::device_count = 0;

extern FileSystemDriver fat32_driver;
extern FileSystemDriver ext4_driver;
// TODO: Add other drivers when implemented
// extern FileSystemDriver ext4_driver;
// extern FileSystemDriver ntfs_driver;

Vector<PendingMount>* FilesystemDetector::pending_mounts;

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
                                      size_t device_size, const char *table_type, bool is_root_device) {
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
    desc.partition_table_type = strdup(table_type);

    MountPoint mp{};
    strncpy(mp.path, suggested_path, sizeof(mp.path) - 1);
    mp.root = root;
    mp.device = new DeviceDescriptor(desc);
    mp.is_virtual = false;
    mp.is_partition = is_partition;
    mp.is_root_device = is_root_device;

    SYS_EVENT_FILESYSTEM_MOUNT(mp.path, fs_info.type_name);

    VFS::add_mount_point(mp);
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

    const char *table_type = nullptr;
    for (size_t i = 0; i < device_count_actual; i++) {
        BlockDevice *device = devices[i];
        if (!device) continue;

        PartitionEntry parts[16];
        size_t pcount = parse_partitions(device, parts, 16);

        uint8_t sector[512];
        if (device->read(1, 1, sector) && memcmp(sector, "EFI PART", 8) == 0) {
            table_type = "GPT";
        } else if (device->read(0, 1, sector) && sector[510] == 0x55 && sector[511] == 0xAA) {
            table_type = "MBR";
        }

        if (pcount == 0) {
            char mount_path[64];
            if (!root_assigned) {
                snprintf(mount_path, sizeof(mount_path), "/");
                if (mount_device(device, mount_path, false, 0, table_type, true)) {
                    root_assigned = true;
                    successful_mounts++;
                }
            } else {
                snprintf(mount_path, sizeof(mount_path), "/mnt/dev%d", i);
                if (root_assigned) {
                    ensure_path_exists(mount_path);
                    if (mount_device(device, mount_path, false, 0, table_type)) {
                        successful_mounts++;
                    }
                } else {
                    // vormerken
                    PendingMount pm{};
                    strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                    pm.device = device;
                    pm.device_size = 0;
                    pm.is_partition = false;
                    pm.table_type = nullptr;
                    pending_mounts->push_back(pm);
                }
            }
            continue;
        }

        for (size_t pi = 0; pi < pcount; ++pi) {
            PartitionEntry &pe = parts[pi];
            auto *pdev = new PartitionDevice(device, pe.start_lba, pe.length_lba);

            // Heuristik: EFI-Partition?
            char mount_path[64];
            bool looks_like_esp = false; {
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
                if (mount_device(pdev, mount_path, true, pe.length_lba, table_type)) {
                    root_assigned = true;
                    successful_mounts++;
                } else {
                    delete pdev;
                }
            } else if (root_assigned) {
                ensure_path_exists(mount_path);
                if (mount_device(pdev, mount_path, true, pe.length_lba, table_type)) {
                    successful_mounts++;
                } else {
                    delete pdev;
                }
            } else {
                PendingMount pm{};
                strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                pm.device = pdev;
                pm.device_size = pe.length_lba;
                pm.is_partition = true;
                pm.table_type = table_type;
                pending_mounts->push_back(pm);
                Log::debug("[FS] Queued mount %s until root is ready", mount_path);
            }
        }
    }

    // Root gefunden? -> nachtragen
    if (root_assigned && !pending_mounts->empty()) {
        for (auto &pm: *pending_mounts) {
            ensure_path_exists(pm.path);
            if (mount_device(pm.device, pm.path, pm.is_partition, pm.device_size, pm.table_type)) {
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

    if (VFS::mount_points_count() == 0) {
        Log::Info("[FS] No devices detected");
        return;
    }

    for (Vector<MountPoint> snapshot = VFS::get_mount_points_snapshot(); auto &mp: snapshot) {
        if (mp.is_virtual) continue;
        const DeviceDescriptor *dev = mp.device;
        if (!dev) continue;

        if (dev->partition_table_type) {
            Log::Info("[FS]   Partition Table: %s", dev->partition_table_type);
        } else {
            Log::Info("[FS]   Partition Table: Unknown");
        }

        // Typ und Status
        if (dev->is_recognized) {
            Log::Info("[FS]   Type: %s (%s)", dev->fs_info.type_name, dev->fs_info.description);
        } else {
            Log::Info("[FS]   Type: Unknown/Unsupported");
        }

        if (dev->fs_info.mounted) {
            Log::Info("[FS]   Status: Mounted at %s", mp.path);
        } else {
            Log::Info("[FS]   Status: Detected but not mounted");
        }

        if (!mp.is_partition) {
            for (auto &part: snapshot) {
                if (part.is_partition && part.device && part.device->device == dev->device) {
                    Log::Info("  Partition: %s", part.path);
                    if (part.device->is_recognized) {
                        Log::Info("    Type: %s (%s)", part.device->fs_info.type_name,
                                  part.device->fs_info.description);
                    } else {
                        Log::Info("    Type: Unknown/Unsupported");
                    }
                    if (part.device->fs_info.mounted) {
                        Log::Info("    Status: Mounted");
                    } else {
                        Log::Info("    Status: Detected but not mounted");
                    }
                }
            };
        }
    };

    Log::Info("[FS] === End of Device List ===");
}
