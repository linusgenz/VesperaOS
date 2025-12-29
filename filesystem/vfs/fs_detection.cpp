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
#include "../../include/kernel/devices/device_manager.h"
#include "vfs.h"
#include <kernel/system/system_manager.h>
#include <log.h>

#include "fs_registry.h"

size_t FilesystemDetector::driver_count = 0;
size_t FilesystemDetector::device_count = 0;

extern FileSystemDriver fat32_driver;
extern FileSystemDriver ext4_driver;
// TODO: Add other drivers when implemented
// extern FileSystemDriver ntfs_driver;

Vector<PendingMount>* FilesystemDetector::pending_mounts;

void FilesystemDetector::Init()
{
    device_count = 0;
    pending_mounts = new Vector<PendingMount>();
}

void FilesystemDetector::RegisterAllDrivers()
{
    // FAT32 driver
    if (fs_driver_count() < MAX_FS_DRIVERS)
    {
        register_fs_driver(&fat32_driver);
    }

    // EXT4 driver
    if (fs_driver_count() < MAX_FS_DRIVERS)
    {
        register_fs_driver(&ext4_driver);
    }
}

bool FilesystemDetector::DetectFilesystem(BlockDevice* device, FilesystemInfo* info)
{
    if (!device || !info) return false;

    // Initialize info structure
    info->type_name = nullptr;
    info->description = nullptr;
    info->mounted = false;

    for (size_t i = 0; i < fs_driver_count(); i++)
    {
        FileSystemDriver* drv = fs_driver_at(i);
        if (drv && drv->probe && drv->probe(device, info))
        {
            info->type_name = drv->name;

            if (strcmp(drv->name, "fat32") == 0)
            {
                info->description = "Microsoft FAT32 Filesystem";
            }
            else if (strcmp(drv->name, "ext4") == 0)
            {
                info->description = "Linux Extended Filesystem v4";
            }
            else if (strcmp(drv->name, "ntfs") == 0)
            {
                info->description = "Microsoft NTFS Filesystem";
            }
            else
            {
                info->description = "Unknown Filesystem";
            }

            return true;
        }
    }

    Log::Warning("[FS] No supported filesystem detected");
    return false;
}

bool FilesystemDetector::Unmount(MountPoint* mp)
{
    if (!mp) return false;

    // If not virtual, unmount via the driver
    if (!mp->is_virtual && mp->root && mp->device && mp->device->fs_info.mounted)
    {
        FileSystemDriver* driver = find_fs_driver(mp->device->fs_info.type_name);
        if (driver && driver->unmount)
        {
            if (!driver->unmount(mp->root))
            {
                Log::Error("[FS] Failed to unmount %s using driver %s", mp->device, driver->name);
                return false;
            }
        }
        mp->device->fs_info.mounted = false;
    }

    // Remove from VFS
    if (!VFS::remove_mount_point(mp))
    {
        Log::Warning("[FS] Failed to remove mount point %s from VFS", mp->path);
        return false;
    }

    Log::Info("[FS] Successfully unmounted %s", mp->path);
    return true;
}

void FilesystemDetector::UnmountAll()
{
    Vector<MountPoint*> snapshot = VFS::get_mount_points_snapshot();
    MountPoint* root_mp = nullptr;

    for (auto& mp : snapshot)
    {
        if (mp->is_root_device)
        {
            root_mp = VFS::find_mount_point(mp->path);
            continue;
        }

        MountPoint* real_mp = VFS::find_mount_point(mp->path);
        if (real_mp)
        {
            Unmount(real_mp);
        }
    }

    if (root_mp)
    {
        Unmount(root_mp);
    }
}

VfsNode* FilesystemDetector::MountFilesystem(BlockDevice* device, FilesystemInfo* fs_info)
{
    if (!device || !fs_info) return nullptr;

    FileSystemDriver* driver = find_fs_driver(fs_info->type_name);
    if (!driver || !driver->mount)
    {
        Log::Error("[FS] No driver for %s", fs_info->type_name);
        return nullptr;
    }

    return driver->mount(device);
}

bool FilesystemDetector::mount_device(BlockDevice* device, const char* suggested_path, bool is_partition,
                                      const char* table_type, bool is_root_device)
{
    FilesystemInfo fs_info{};
    if (!DetectFilesystem(device, &fs_info))
    {
        Log::Warning("[FS] No supported filesystem detected on %s", suggested_path);
        return false;
    }

    VfsNode* root = MountFilesystem(device, &fs_info);
    if (!root) return false;

    if (strcmp(suggested_path, "/") != 0)
    {
        VFS::ensure_path_exists(suggested_path);
    }

    fs_info.mounted = true;

    DeviceDescriptor desc{};
    desc.device = device;
    desc.device_size = device->get_size();
    desc.is_recognized = true;
    desc.fs_info = fs_info;
    desc.partition_table_type = strdup(table_type);

    auto* mp = new MountPoint();
    strncpy(mp->path, suggested_path, sizeof(mp->path) - 1);
    mp->root = root;
    mp->device = new DeviceDescriptor(desc);
    mp->is_virtual = false;
    mp->is_partition = is_partition;
    mp->is_root_device = is_root_device;

    SYS_EVENT_FILESYSTEM_MOUNT(mp->path, fs_info.type_name);

    VFS::add_mount_point(mp);
    Log::debug("mount device end, mounted: %s", mp->path);
    return true;
}


void FilesystemDetector::ScanAndMountAll()
{
    auto devices = DeviceManager::GetAllDevices();
    size_t device_count_actual = DeviceManager::GetDeviceCount();

    if (device_count_actual == 0)
    {
        Log::Warning("[FS] No storage devices found");
        return;
    }

    int successful_mounts = 0;
    static bool root_assigned = false;

    const char* table_type = nullptr;
    for (size_t i = 0; i < devices.size(); ++i)
    {
        const KernelDevice* kd = devices[i];

        if (!kd || kd->type != DeviceType::Block) continue;
        BlockDevice* device = kd->block;
        if (!device) continue;

        if (device->type == BlockDevice::Type::Disk && kd->children.empty())
        {
            char mount_path[64];
            if (!root_assigned)
            {
                snprintf(mount_path, sizeof(mount_path), "/");
                if (mount_device(device, mount_path, false, table_type, true))
                {
                    root_assigned = true;
                    successful_mounts++;
                }
            }
            else
            {
                snprintf(mount_path, sizeof(mount_path), "/mnt/dev%d", i);
                if (root_assigned)
                {
                    VFS::ensure_path_exists(mount_path);
                    if (mount_device(device, mount_path, false, table_type, false))
                    {
                        successful_mounts++;
                    }
                    else
                    {
                        VFS::rmdir(mount_path);
                    }
                }
                else
                {
                    // remember
                    PendingMount pm{};
                    strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                    pm.device = device;
                    pm.device_size = 0;
                    pm.is_partition = false;
                    pm.table_type = nullptr;
                    pending_mounts->push_back(pm);
                }
            }
        }
        else if (kd->block->type == BlockDevice::Type::Partition)
        {
            {
                if (kd->type != DeviceType::Block)
                    continue;

                BlockDevice* pdev = kd->block;
                if (!pdev)
                    continue;

                FilesystemInfo fs_info{};
                if (DetectFilesystem(pdev, &fs_info))
                {
                    const char* label = fs_info.label;

                    bool is_esp = false;
                    bool is_root = false;

                    if (label)
                    {
                        if (strncmp(label, "VesperaEFI", strlen("VesperaEFI")) == 0)
                            is_esp = true;
                        else if (strncmp(label, "VesperaRoot", strlen("VesperaRoot")) == 0)
                            is_root = true;
                    }

                    char mount_path[64];
                    if (is_root && !root_assigned)
                    {
                        snprintf(mount_path, sizeof(mount_path), "/");
                        if (mount_device(pdev, "/", true, nullptr, true))
                        {
                            root_assigned = true;
                            successful_mounts++;
                        }
                        continue;
                    }

                    if (is_esp)
                        snprintf(mount_path, sizeof(mount_path), "/efi");
                    else
                        snprintf(mount_path, sizeof(mount_path), "/mnt/%s", kd->name ? kd->name : "part");

                    if (root_assigned)
                    {
                        VFS::ensure_path_exists(mount_path);
                        if (mount_device(pdev, mount_path, true, nullptr, false))
                            successful_mounts++;
                        else
                            VFS::rmdir(mount_path);
                    }
                    else
                    {
                        PendingMount pm{};
                        strncpy(pm.path, mount_path, sizeof(pm.path) - 1);
                        pm.device = pdev;
                        pm.device_size = 0;
                        pm.is_partition = true;
                        pm.table_type = nullptr;
                        pending_mounts->push_back(pm);
                    }
                }
            }
        }
    }

    if (root_assigned)
    {
        for (const auto& pm : *pending_mounts)
        {
            VFS::ensure_path_exists(pm.path);
            if (mount_device(pm.device, pm.path, pm.is_partition, pm.table_type, false))
                successful_mounts++;
            else
                VFS::rmdir(pm.path);
        }
        pending_mounts->clear();

        for (MountPoint* mp : VFS::get_mount_points_snapshot())
        {
            if (mp->is_virtual)
                VFS::mkdir(mp->path);
        }
    }


    if (successful_mounts == 0)
        Log::Warning("[FS] No filesystems could be mounted automatically");
}

void FilesystemDetector::PrintDetectedFilesystems()
{
    Log::Info("[FS] === Detected Storage Devices ===");

    if (VFS::mount_points_count() == 0)
    {
        Log::Info("[FS] No devices detected");
        return;
    }

    for (Vector<MountPoint*> snapshot = VFS::get_mount_points_snapshot(); auto& mp : snapshot)
    {
        if (mp->is_virtual) continue;
        const DeviceDescriptor* dev = mp->device;
        if (!dev) continue;

        if (dev->partition_table_type)
        {
            Log::Info("[FS]   Partition Table: %s", dev->partition_table_type);
        }
        else
        {
            Log::Info("[FS]   Partition Table: Unknown");
        }

        // Typ und Status
        if (dev->is_recognized)
        {
            Log::Info("[FS]   Type: %s (%s)", dev->fs_info.type_name, dev->fs_info.description);
        }
        else
        {
            Log::Info("[FS]   Type: Unknown/Unsupported");
        }

        if (dev->fs_info.mounted)
        {
            Log::Info("[FS]   Status: Mounted at %s", mp->path);
        }
        else
        {
            Log::Info("[FS]   Status: Detected but not mounted");
        }

        if (!mp->is_partition)
        {
            for (auto& part : snapshot)
            {
                if (part->is_partition && part->device && part->device->device == dev->device)
                {
                    Log::Info("  Partition: %s", part->path);
                    if (part->device->is_recognized)
                    {
                        Log::Info("    Type: %s (%s)", part->device->fs_info.type_name,
                                  part->device->fs_info.description);
                    }
                    else
                    {
                        Log::Info("    Type: Unknown/Unsupported");
                    }
                    if (part->device->fs_info.mounted)
                    {
                        Log::Info("    Status: Mounted");
                    }
                    else
                    {
                        Log::Info("    Status: Detected but not mounted");
                    }
                }
            }
        }
    }

    Log::Info("[FS] === End of Device List ===");
}
