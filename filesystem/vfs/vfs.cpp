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
#include <klib/path.h>
#include <klib/string.h>
#include <vespera/devices/device_manager.h>
#include <vespera/filesystem/vfs.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera_errno.h>

#include "fs_detection.h"
#include "uapi/vespera/mount.h"
#include "vfs_node.h"

Vector<MountPoint*>* VFS::mount_points_ = nullptr;
Spinlock VFS::mount_points_lock_;

void VFS::init() {
    mount_points_ = new Vector<MountPoint*>();
    mount_points_lock_.init("mount_points_lock");

    FilesystemDetector::init();

    FilesystemDetector::register_all_drivers();

    // FilesystemDetector::ScanAndMountAll();

    // FilesystemDetector::PrintDetectedFilesystems();
}

VfsNode* VFS::mount_virtual(VfsNode* root, const char* mount_path) {
    if (!root || !mount_path) return nullptr;

    auto* mp = new MountPoint();
    strncpy(mp->path, mount_path, sizeof(mp->path) - 1);
    mp->path[sizeof(mp->path) - 1] = '\0';
    mp->is_virtual = true;
    mp->root = root;
    mp->root->mount = mp;

    {
        SpinlockGuard g(mount_points_lock_);
        mount_points_->push_back(mp);
    }

    return root;
}

VfsNode* VFS::open(const char* path) {
    if (!path || !*path) return nullptr;

    // --- Mountpoint-Suche ---
    const MountPoint* best_match = nullptr;
    usize best_len = 0;

    {
        SpinlockGuard g(mount_points_lock_);
        for (const auto& mp : *mount_points_) {
            if (const usize len = strlen(mp->path);
                strncmp(path, mp->path, len) == 0 &&
                (strcmp(mp->path, "/") == 0 || path[len] == '/' || path[len] == '\0') && len > best_len) {
                best_match = mp;
                best_len = len;
            }
        }
    }

    if (!best_match) return nullptr;

    const char* sub_path = path + best_len;
    if (*sub_path == '/') sub_path++;

    Log::debug("bm: %p, bmr: %p, bmro: %p, bmrof: %p", best_match, best_match->root, best_match->root->ops, best_match->root->ops->find);
    if (!best_match->root->ops || !best_match->root->ops->find) return nullptr;

    VfsNode* current = best_match->root;
    current->mount = best_match;

    char components[16][32];
    const usize count = split_path(sub_path, components, 16);

    for (usize i = 0; i < count; i++) {
        current = current->ops->find(current, components[i]);
        if (!current) return nullptr;
    }

    return current;
}

int VFS::opendir(VfsNode* node, VfsDir** out_dir) {
    if (!node) return -EINVAL;

    if (node->type != VfsNodeType::Directory) return -ENOTDIR;

    if (!node->ops || !node->ops->opendir) return -ENOSYS;

    void* handle = node->ops->opendir(node);
    if (!handle) return -EIO;

    auto* dir = static_cast<VfsDir*>(kernel::memory::malloc(sizeof(VfsDir)));
    if (!dir) return -ENOMEM;

    dir->node = node;
    dir->handle = handle;

    *out_dir = dir;
    return 0;
}

int VFS::readdir(const VfsDir* dir, dirent_t* out) {
    if (!dir || !dir->node || !dir->node->ops || !dir->node->ops->readdir) return -ENOSYS;
    return dir->node->ops->readdir(dir->handle, out);
}

void VFS::close(VfsNode* node) {
    if (!node || !node->ops || !node->ops->close || node->permanent) return;
    node->ops->close(node);
}

void VFS::closedir(VfsDir* dir) {
    if (!dir) return;
    if (dir->node && dir->node->ops && dir->node->ops->closedir && dir->handle) {
        dir->node->ops->closedir(dir->handle);
    }
    if (dir->node) close(dir->node);
    kernel::memory::free(dir);
}

isize VFS::read(const VfsNode* node, const usize offset, const usize size, void* buffer) {
    if (!node || !node->ops || !node->ops->read) return -ENOSYS;
    return node->ops->read(node, offset, size, buffer);
}

static bool is_read_only(const VfsNode* node) {
    if (!node->mount) return false;
    return (node->mount->flags & MS_RDONLY);
}

isize VFS::write(VfsNode* node, const usize offset, const usize size, const void* buffer) {
    Log::debug("n: %p, %p %p", node, node->ops, node->ops->write);
    if (!node || !node->ops || !node->ops->write) return -ENOSYS;

    if (is_read_only(node)) {
        return -EROFS;
    }

    return node->ops->write(node, offset, size, buffer);
}

int VFS::create(const char* path) {
    if (!path) return -EINVAL;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (is_read_only(parent)) {
        return -EROFS;
    }

    if (!parent->ops || !parent->ops->create) {
        close(parent);
        return -ENOSYS;
    }

    const int result = parent->ops->create(parent, name);
    close(parent);
    return result;
}

int VFS::rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return -EINVAL;

    VfsNode *old_parent = nullptr, *new_parent = nullptr;
    char old_name[64], new_name[64];

    if (!resolve_parent(old_path, &old_parent, old_name)) return -ENOENT;
    if (!resolve_parent(new_path, &new_parent, new_name)) {
        close(old_parent);
        return -ENOENT;
    }

    if (is_read_only(old_parent)) {
        return -EROFS;
    }

    if (old_parent != new_parent) {
        close(old_parent);
        close(new_parent);
        return -EXDEV;
    }

    if (!old_parent->ops || !old_parent->ops->rename) {
        close(old_parent);
        close(new_parent);
        return -ENOSYS;
    }

    const int status = old_parent->ops->rename(old_parent, old_name, new_name);
    close(old_parent);
    close(new_parent);
    return status;
}

int VFS::mkdir(const char* path) {
    if (!path) return -EINVAL;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (is_read_only(parent)) {
        return -EROFS;
    }

    if (!parent->ops || !parent->ops->mkdir) {
        close(parent);
        return -ENOSYS;
    }

    const int result = parent->ops->mkdir(parent, name);
    close(parent);
    return result;
}

int VFS::rmdir(const char* path) {
    if (!path) return -EINVAL;

    {
        SpinlockGuard guard(mount_points_lock_);
        for (const auto& mp : *mount_points_) {
            if (strcmp(mp->path, path) == 0) return -EPERM;
        }
    }

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (is_read_only(parent)) {
        return -EROFS;
    }

    if (!parent->ops || !parent->ops->rmdir) {
        close(parent);
        return -ENOSYS;
    }

    const int result = parent->ops->rmdir(parent, name);
    close(parent);
    return result;
}

int VFS::unlink(const char* path) {
    if (!path) return -EINVAL;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (is_read_only(parent)) {
        return -EROFS;
    }

    if (!parent->ops || !parent->ops->unlink) {
        close(parent);
        return -ENOSYS;
    }

    const int result = parent->ops->unlink(parent, name);
    close(parent);
    return result;
}

int VFS::truncate(VfsNode* node, const usize new_size) {
    if (!node ||!node->ops || !node->ops->truncate) {
        return -ENOSYS;
    }

    if (is_read_only(node)) {
        return -EROFS;
    }

    return node->ops->truncate(node, new_size);
}
/*
bool VFS::probe_filesystem(BlockDevice* device) {
    FilesystemInfo info{};
    return FilesystemDetector::detect_filesystem(device, &info);
}*/

void VFS::list_devices() {
    FilesystemDetector::print_detected_filesystems();
}

void VFS::remount_all() {
    Log::info("[VFS] Remounting all detected devices...");
    FilesystemDetector::init();
    FilesystemDetector::register_all_drivers();
    FilesystemDetector::scan_and_mount_all();
    FilesystemDetector::print_detected_filesystems();
}
/*
void VFS::get_stats(VfsStats* stats) {
    if (!stats) return;

    auto devices = DeviceManager::query([](const KernelDevice* kd) { return kd->block != nullptr; });

    stats->total_devices = devices.size();
    stats->mounted_devices = 0;
    stats->supported_filesystems = 0;

    for (const KernelDevice* dev : devices) {
        FilesystemInfo info;
        if (FilesystemDetector::detect_filesystem(dev->block, &info) && info.mounted) {
            stats->mounted_devices++;
        }
    }

    // Count supported filesystem types
    // For now, just count the registered drivers
    stats->supported_filesystems = 1;  // FAT32 is always supported
    // TODO: Add count of other registered drivers when implemented
}*/

void VFS::add_mount_point(MountPoint* mp) {
    SpinlockGuard g(mount_points_lock_);
    mount_points_->push_back(mp);
}

usize VFS::mount_points_count() {
    SpinlockGuard g(mount_points_lock_);
    return mount_points_->size();
}

MountPoint* VFS::find_mount_point(const char* path) {
    if (!path) return nullptr;

    char norm[256];
    normalize_path(path, norm, sizeof(norm));
    strip_trailing_slash(norm);

    SpinlockGuard g(mount_points_lock_);

    for (const auto& mp : *mount_points_) {
        char mp_norm[256];
        normalize_path(mp->path, mp_norm, sizeof(mp_norm));
        strip_trailing_slash(mp_norm);

        if (strcmp(mp_norm, norm) == 0) {
            return mp;
        }
    }

    return nullptr;
}

bool VFS::remove_mount_point(const MountPoint* mp) {
    if (!mp) return false;
    SpinlockGuard g(mount_points_lock_);

    for (usize i = 0; i < mount_points_->size(); ++i) {
        if (MountPoint* mp_iter = (*mount_points_)[i]; mp_iter == mp) {
            if (mp_iter->device) {
                delete mp_iter->device;
                mp_iter->device = nullptr;
            }
            mount_points_->erase(i);
            return true;
        }
    }
    return false;  // Not found
}
