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

#include <filesystem/vfs.h>
#include <klib/string.h>
#include <vespera/devices/device_manager.h>
#include <vespera/log.h>
#include <vespera/system/system_manager.h>
#include <vespera_errno.h>

#include "../kernel/devices/partition_device.h"
#include "fs_registry.h"

usize FilesystemDetector::driver_count_ = 0;
usize FilesystemDetector::device_count_ = 0;

extern FileSystemDriver fat32_driver;
extern FileSystemDriver ext4_driver;
// TODO: Add other drivers when implemented
// extern FileSystemDriver ntfs_driver;

struct PendingMount {
    char path[64];
    BlkDeviceDescriptor desc;
    bool is_partition;
    const char* table_type;
};

void FilesystemDetector::init() {
    device_count_ = 0;
}

void FilesystemDetector::register_all_drivers() {
    // FAT32 driver
    if (fs_driver_count() < MAX_FS_DRIVERS) {
        register_fs_driver(&fat32_driver);
    }

    // EXT4 driver
    if (fs_driver_count() < MAX_FS_DRIVERS) {
        register_fs_driver(&ext4_driver);
    }
}

bool FilesystemDetector::detect_filesystem(BlkDeviceDescriptor* blk_desc) {
    if (!blk_desc) return false;
    FilesystemInfo* info = &blk_desc->fs_info;

    // Initialize info structure
    info->type_name = nullptr;
    info->description = nullptr;
    info->mounted = false;

    for (usize i = 0; i < fs_driver_count(); i++) {
        if (const FileSystemDriver* drv = fs_driver_at(i); drv && drv->probe && drv->probe(blk_desc->device, info)) {
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

            return true;
        }
    }

    Log::warning("[FS] No supported filesystem detected");
    return false;
}

bool FilesystemDetector::unmount(MountPoint* mp) {
    if (!mp) return false;

    // If not virtual, unmount via the driver
    if (!mp->is_virtual && mp->root && mp->device && mp->device->fs_info.mounted) {
        if (const FileSystemDriver* driver = find_fs_driver(mp->device->fs_info.type_name); driver && driver->unmount) {
            if (!driver->unmount(mp->root)) {
                Log::error("[FS] Failed to unmount %s using driver %s", mp->device, driver->name);
                return false;
            }
        }
        mp->device->fs_info.mounted = false;
    }

    // Remove from VFS
    if (!VFS::remove_mount_point(mp)) {
        Log::warning("[FS] Failed to remove mount point %s from VFS", mp->path);
        return false;
    }

    return true;
}

void FilesystemDetector::unmount_all() {
    Vector<MountPoint*> snapshot = VFS::get_mount_points_snapshot();
    MountPoint* root_mp = nullptr;

    for (const auto& mp : snapshot) {
        if (mp->is_root_device) {
            root_mp = VFS::find_mount_point(mp->path);
            continue;
        }

        if (MountPoint* real_mp = VFS::find_mount_point(mp->path)) {
            unmount(real_mp);
        }
    }

    if (root_mp) {
        unmount(root_mp);
    }
}

void FilesystemDetector::emergency_detach_device(const BlockDevice* device) {
    if (!device) return;

    Vector<MountPoint*> snapshot = VFS::get_mount_points_snapshot();

    for (const auto& mp : snapshot) {
        if (mp->is_virtual || !mp->device || !mp->device->device) continue;

        BlockDevice* mp_device = mp->device->device;

        bool is_affected = (mp_device == device);

        if (!is_affected && mp->is_partition) {
            const auto* part = static_cast<PartitionDevice*>(mp_device);
            is_affected = (part->get_parent() == device);
        }

        if (!is_affected) continue;

        if (mp->device->fs_info.mounted) {
            if (const FileSystemDriver* driver = find_fs_driver(mp->device->fs_info.type_name);
                driver && driver->force_unmount) {
                driver->force_unmount(mp->root);
            }
            mp->device->fs_info.mounted = false;
        }

        VFS::remove_mount_point(mp);

        Log::warning("[FS] Emergency detach: forcefully unmounted %s", mp->path);
    }
}

VfsNode* FilesystemDetector::mount_filesystem(const BlkDeviceDescriptor* blk_desc) {
    const FileSystemDriver* driver = find_fs_driver(blk_desc->fs_info.type_name);
    if (!driver || !driver->mount) {
        Log::error("[FS] No driver for %s", blk_desc->fs_info.type_name);
        return nullptr;
    }

    return driver->mount(blk_desc->device);
}

bool FilesystemDetector::mount_device(
    BlkDeviceDescriptor* blk_desc, const char* suggested_path, const bool is_partition, const bool is_root_device
) {
    if (!detect_filesystem(blk_desc)) {
        Log::warning("[FS] No supported filesystem detected on %s", suggested_path);
        return false;
    }

    VfsNode* root = mount_filesystem(blk_desc);
    if (!root) return false;

    if (strcmp(suggested_path, "/") != 0) {
        VFS::ensure_path_exists(suggested_path);
    }

    blk_desc->fs_info.mounted = true;
    blk_desc->is_recognized = true;

    auto* mp = new MountPoint();
    strncpy(mp->path, suggested_path, sizeof(mp->path) - 1);
    mp->root = root;
    mp->root->mount = mp;
    mp->device = new BlkDeviceDescriptor(*blk_desc);
    mp->is_virtual = false;
    mp->is_partition = is_partition;
    mp->is_root_device = is_root_device;

    SYS_EVENT_FILESYSTEM_MOUNT(mp->path, blk_desc->fs_info.type_name);

    VFS::add_mount_point(mp);
    return true;
}

void FilesystemDetector::scan_and_mount_all() {
    auto devices = DeviceManager::query([](const KernelDevice* kd) { return kd->block != nullptr; });

    for (const auto& device : devices) {
        DeviceManager::find_and_register_partitions(device);
    }

    devices = DeviceManager::query([](const KernelDevice* kd) { return kd->block != nullptr; });

    if (const usize device_count_actual = devices.size(); device_count_actual == 0) {
        Log::warning("[FS] No storage devices found");
        return;
    }

    int successful_mounts = 0;
    static bool root_assigned = false;

    auto pending_mounts = Vector<PendingMount>{};

    for (usize i = 0; i < devices.size(); ++i) {
        const KernelDevice* kd = devices[i];
        BlockDevice* blk = kd->block;

        auto desc = BlkDeviceDescriptor{};
        desc.device = blk;
        desc.device_id = kd->id;
        desc.device_size = blk->get_size();
        desc.is_recognized = false;
        desc.partition_table_type = nullptr;

        if (blk->type == BlockDevice::Type::Disk && kd->children.empty()) {
            char mount_path[64];

            snprintf(mount_path, sizeof(mount_path), "/mnt/dev%d", i);
            if (root_assigned) {
                VFS::ensure_path_exists(mount_path);
                if (mount_device(&desc, mount_path, false, false)) {
                    successful_mounts++;
                } else {
                    VFS::rmdir(mount_path);
                }
            } else {
                // remember
                auto pm = PendingMount{};
                strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                pm.desc = desc;
                pm.is_partition = false;
                pm.table_type = nullptr;
                pending_mounts.push_back(pm);
            }
        } else if (kd->block->type == BlockDevice::Type::Partition) {
            {
                if (detect_filesystem(&desc)) {
                    const char* label = desc.fs_info.label;

                    bool is_esp = false;
                    bool is_root = false;

                    if (label) {
                        if (strncmp(label, "VesperaEFI", strlen("VesperaEFI")) == 0)
                            is_esp = true;
                        else if (strncmp(label, "VesperaRoot", strlen("VesperaRoot")) == 0)
                            is_root = true;
                        else
                            continue;
                    }

                    char mount_path[64];
                    if (is_root && !root_assigned) {
                        snprintf(mount_path, sizeof(mount_path), "/");
                        if (mount_device(&desc, "/", true, true)) {
                            root_assigned = true;
                            successful_mounts++;
                        }
                        continue;
                    }

                    if (is_esp)
                        snprintf(mount_path, sizeof(mount_path), "/efi");
                    else
                        snprintf(mount_path, sizeof(mount_path), "/mnt/%s", kd->name ? kd->name : "part");

                    if (root_assigned) {
                        VFS::ensure_path_exists(mount_path);
                        if (mount_device(&desc, mount_path, true, false))
                            successful_mounts++;
                        else
                            VFS::rmdir(mount_path);
                    } else {
                        auto pm = PendingMount{};
                        strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                        pm.desc = desc;
                        pm.is_partition = true;
                        pm.table_type = nullptr;
                        pending_mounts.push_back(pm);
                    }
                } else {
                    Log::debug("partition has incompatible fs");
                }
            }
        }
    }

    if (root_assigned) {
        for (auto& pm : pending_mounts) {
            VFS::ensure_path_exists(pm.path);
            if (mount_device(&pm.desc, pm.path, pm.is_partition, false))
                successful_mounts++;
            else
                VFS::rmdir(pm.path);
        }
        pending_mounts.clear();

        for (const MountPoint* mp : VFS::get_mount_points_snapshot()) {
            if (mp->is_virtual) VFS::mkdir(mp->path, 0755);
        }
    }

    if (successful_mounts == 0) Log::warning("[FS] No filesystems could be mounted automatically");
}

i64 FilesystemDetector::mount_manual(const KernelDevice* device, const char* target, const char* fstype, u64 flags) {
    if (!target) return -EINVAL;

    auto desc = BlkDeviceDescriptor{};
    desc.device = device->block;
    desc.device_id = device->id;
    desc.device_size = device->block ? device->block->get_size() : 0;
    desc.is_recognized = true;
    desc.partition_table_type = nullptr;

    if (fstype) {
        const FileSystemDriver* drv = find_fs_driver(fstype);
        if (!drv) return -EINVAL;

        desc.fs_info.type_name = drv->name;
    } else {
        if (!detect_filesystem(&desc)) return -EUNSUPPORTED;
    }

    VfsNode* root = mount_filesystem(&desc);
    if (!root) return -EIO;

    if (strcmp(target, "/") != 0) VFS::ensure_path_exists(target);

    desc.fs_info.mounted = true;

    auto* mp = new MountPoint();
    strncpy(mp->path, target, sizeof(mp->path) - 1);
    mp->root = root;
    mp->root->mount = mp;
    mp->device = new BlkDeviceDescriptor(desc);
    mp->is_virtual = (device == nullptr);
    mp->is_partition = false;
    mp->is_root_device = (strcmp(target, "/") == 0);
    mp->flags = flags;

    VFS::add_mount_point(mp);

    return SUCCESS_CODE;
}

void FilesystemDetector::print_detected_filesystems() {
    Log::info("[FS] === Detected Storage Devices ===");

    if (VFS::mount_points_count() == 0) {
        Log::info("[FS] No devices detected");
        return;
    }

    for (Vector<MountPoint*> snapshot = VFS::get_mount_points_snapshot(); const auto& mp : snapshot) {
        if (mp->is_virtual) continue;
        const BlkDeviceDescriptor* dev = mp->device;
        if (!dev) continue;

        if (dev->partition_table_type) {
            Log::info("[FS]   Partition Table: %s", dev->partition_table_type);
        } else {
            Log::info("[FS]   Partition Table: Unknown");
        }

        // Typ und Status
        if (dev->is_recognized) {
            Log::info("[FS]   Type: %s (%s)", dev->fs_info.type_name, dev->fs_info.description);
        } else {
            Log::info("[FS]   Type: Unknown/Unsupported");
        }

        if (dev->fs_info.mounted) {
            Log::info("[FS]   Status: Mounted at %s", mp->path);
        } else {
            Log::info("[FS]   Status: Detected but not mounted");
        }

        if (!mp->is_partition) {
            for (const auto& part : snapshot) {
                if (part->is_partition && part->device && part->device->device == dev->device) {
                    Log::info("  Partition: %s", part->path);
                    if (part->device->is_recognized) {
                        Log::info(
                            "    Type: %s (%s)", part->device->fs_info.type_name, part->device->fs_info.description
                        );
                    } else {
                        Log::info("    Type: Unknown/Unsupported");
                    }
                    if (part->device->fs_info.mounted) {
                        Log::info("    Status: Mounted");
                    } else {
                        Log::info("    Status: Detected but not mounted");
                    }
                }
            }
        }
    }

    Log::info("[FS] === End of Device List ===");
}
