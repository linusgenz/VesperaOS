// vfs.cpp
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
#include "vfs_node.h"
#include "../../include/path.h"
#include "../../include/string.h"
#include "vfs.h"
#include "fs_registry.h"
#include "../../kernel/include/errno.h"
#include "vfs_helper.h"
#include "../fat32/fat32_vfs_adapter.h"
#include "../../kernel/devices/device_manager.h"
#include "fs_detection.h"

Vector<MountPoint> *mount_points;

void vfs_init() {
    mount_points = new Vector<MountPoint>();

    FilesystemDetector::Init();

    FilesystemDetector::RegisterAllDrivers();

    FilesystemDetector::ScanAndMountAll();

    //    FilesystemDetector::PrintDetectedFilesystems();

    //   Log::Info("[VFS] Initialization complete");
}

VfsNode *vfs_mount(BlockDevice *device, const char *mount_path) {
    if (!device) {
        Log::Error("[VFS] Cannot mount: Invalid device");
        return nullptr;
    }

    VfsNode *result = FilesystemDetector::TryMount(device, mount_path);
    if (result) {
        // Log::Info("[VFS] Manual mount successful");
    } else {
        //   Log::Warning("[VFS] Manual mount failed");
    }

    return result;
}

VfsNode *vfs_mount_virtual(VfsNode *root, const char *mount_path) {
    if (!root || !mount_path) return nullptr;

    // Neuen MountPoint eintragen
    MountPoint mp{};
    strncpy(mp.path, mount_path, sizeof(mp.path) - 1);
    mp.is_virtual = true;
    mp.path[sizeof(mp.path) - 1] = '\0';
    mp.root = root;

    mount_points->push_back(mp);

    return root;
}


VfsNode *vfs_open(const char *path) {
    MountPoint *best_match = nullptr;
    size_t best_len = 0;

    for (size_t i = 0; i < mount_points->size(); i++) {
        MountPoint &mp = (*mount_points)[i];
        size_t len = strlen(mp.path);

        if (strncmp(path, mp.path, len) == 0 &&
            (path[len] == '/' || path[len] == '\0') &&
            len > best_len) {
            best_match = &mp;
            best_len = len;
        }
    }

    if (!best_match) return nullptr;

    const char *sub_path = path + best_len;
    if (*sub_path == '/') sub_path++;

    if (!best_match->root->ops || !best_match->root->ops->find) return nullptr;

    VfsNode *current = best_match->root;

    char components[16][32];
    size_t count = split_path(sub_path, components, 16);
    for (size_t i = 0; i < count; i++) {
        current = current->ops->find(current, components[i]);
        if (!current) return nullptr;
    }

    return current;
}

VfsDir *vfs_opendir(const char *path) {
    VfsNode *node = vfs_open(path);
    if (!node || node->type != VfsNodeType::Directory) return nullptr;
    if (!node->ops || !node->ops->opendir) return nullptr;

    void *handle = node->ops->opendir(node);
    if (!handle) {
        vfs_close(node);
        return nullptr;
    }

    auto *dir = (VfsDir *) malloc(sizeof(VfsDir));
    dir->node = node;
    dir->handle = handle;
    return dir;
}


int vfs_readdir(VfsDir *dir, char *out_name, size_t max_len) {
    if (!dir || !dir->node || !dir->node->ops || !dir->node->ops->readdir) return 0;
    return dir->node->ops->readdir(dir->handle, out_name, max_len);
}

void vfs_closedir(VfsDir *dir) {
    if (!dir) return;
    if (dir->node && dir->node->ops && dir->node->ops->closedir && dir->handle) {
        dir->node->ops->closedir(dir->handle);
    }
    if (dir->node) vfs_close(dir->node);
    free(dir);
}

size_t vfs_read(VfsNode *node, size_t offset, size_t size, void *buffer) {
    if (!node || !node->ops || !node->ops->read) return 0;
    return node->ops->read(node, offset, size, buffer);
}

size_t vfs_file_size(VfsNode *file) {
    if (!file) return -EINVAL;

    return file->ops->file_size(file);
}

void vfs_close(VfsNode *node) {
    if (!node || !node->ops || !node->ops->close || node->permanent) return;
    node->ops->close(node);
}

int vfs_rename(const char *oldPath, const char *newPath) {
    if (!oldPath || !newPath) return -EINVAL;

    VfsNode *oldParent;
    VfsNode *newParent;
    char oldName[64];
    char newName[64];

    if (!vfs_resolve_parent(oldPath, &oldParent, oldName)) return -ENOENT;
    if (!vfs_resolve_parent(newPath, &newParent, newName)) return -ENOENT;

    if (oldParent != newParent) {
        vfs_close(oldParent);
        vfs_close(newParent);
        return -EXDEV; // Cross-directory renaming not supported
    }

    if (!oldParent->ops || !oldParent->ops->rename) {
        vfs_close(oldParent);
        vfs_close(newParent);
        return -ENOSYS;
    }

    int status = oldParent->ops->rename(oldParent, oldName, newName);
    vfs_close(oldParent);
    vfs_close(newParent);
    return status;
}

int vfs_create(const char *path) {
    VfsNode *parent;
    char name[64];
    if (!vfs_resolve_parent(path, &parent, name)) return -1;

    if (!parent->ops || !parent->ops->create) {
        vfs_close(parent);
        return -2;
    }

    int result = parent->ops->create(parent, name);
    vfs_close(parent);
    return result;
}


int vfs_mkdir(const char *path) {
    VfsNode *parent;
    char name[64];
    if (!vfs_resolve_parent(path, &parent, name)) return -1;

    if (!parent->ops || !parent->ops->mkdir) {
        vfs_close(parent);
        return -2;
    }

    int result = parent->ops->mkdir(parent, name);
    vfs_close(parent);
    return result;
}


int vfs_rmdir(const char *path) {
    VfsNode *parent;
    char name[64];
    if (!vfs_resolve_parent(path, &parent, name)) return -1;

    if (!parent->ops || !parent->ops->rmdir) {
        vfs_close(parent);
        return -2;
    };
    int result = parent->ops->rmdir(parent, name);
    vfs_close(parent);
    return result;
}

int vfs_unlink(const char *path) {
    VfsNode *parent;
    char name[64];
    if (!vfs_resolve_parent(path, &parent, name)) return -1;

    if (!parent->ops || !parent->ops->unlink) {
        vfs_close(parent);
        return -2;
    };

    int result = parent->ops->unlink(parent, name);
    vfs_close(parent);
    return result;
}

VfsNode *vfs_mount_device(BlockDevice *device, const char *mount_path) {
    if (!device) {
        Log::Error("[VFS] Cannot mount: Invalid device");
        return nullptr;
    }

    VfsNode *result = FilesystemDetector::TryMount(device, mount_path);
    if (result) {
        Log::Info("[VFS] Manual mount successful");
    } else {
        Log::Warning("[VFS] Manual mount failed");
    }

    return result;
}

bool vfs_probe_filesystem(BlockDevice *device) {
    FilesystemInfo info{};
    return FilesystemDetector::DetectFilesystem(device, &info);
}

void vfs_list_devices() {
    FilesystemDetector::PrintDetectedFilesystems();
}

void vfs_remount_all() {
    Log::Info("[VFS] Remounting all detected devices...");

    // Clear existing state
    FilesystemDetector::Init();
    FilesystemDetector::RegisterAllDrivers();

    // Scan and mount again
    FilesystemDetector::ScanAndMountAll();
    FilesystemDetector::PrintDetectedFilesystems();
}

void vfs_get_stats(VfsStats *stats) {
    if (!stats) return;

    stats->total_devices = kernel::DeviceManager::GetDeviceCount();
    stats->mounted_devices = 0;
    stats->supported_filesystems = 0;

    // Count mounted devices (this is a simplified count)
    auto devices = kernel::DeviceManager::GetDevices();
    for (size_t i = 0; i < stats->total_devices; i++) {
        FilesystemInfo info;
        if (FilesystemDetector::DetectFilesystem(devices[i], &info) && info.mounted) {
            stats->mounted_devices++;
        }
    }

    // Count supported filesystem types
    // For now, just count the registered drivers
    stats->supported_filesystems = 1; // FAT32 is always supported
    // TODO: Add count of other registered drivers when implemented
}
