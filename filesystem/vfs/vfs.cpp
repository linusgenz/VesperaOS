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
#include <filesystem/vfs.h>
#include <filesystem/vfs_node.h>
#include <klib/path.h>
#include <klib/string.h>
#include <vespera/devices/device_manager.h>
#include <vespera/log.h>

#include "dentry_cache.h"
#include "fs_detection.h"
#include "uapi/vespera/mount.h"

Vector<MountPoint*>* VFS::mount_points_ = nullptr;
Spinlock VFS::mount_points_lock_;

void VFS::init() {
    mount_points_ = new Vector<MountPoint*>();
    mount_points_lock_.init("mount_points_lock");

    filesystem::g_dentry_cache.init();

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

static bool is_read_only(const VfsNode* node) {
    if (!node->mount) return false;
    return (node->mount->flags & MS_RDONLY);
}

VfsNode* ref_node(VfsNode* node) {
    if (!node) return nullptr;

    __atomic_fetch_add(&node->ref_count, 1, __ATOMIC_RELAXED);
    return node;
}

void unref_node(VfsNode* node) {
    VFS::close(node);
}

Result<VfsNode*> VFS::open(const char* path) {
    if (!path || !*path) return Error::Inval;

    // Look in cache first
    if (VfsNode* cached = filesystem::g_dentry_cache.lookup(path)) return Result<VfsNode*>::ok(cached);

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

    if (!best_match) return Error::NoEnt;

    const char* sub_path = path + best_len;
    if (*sub_path == '/') sub_path++;

    if (!best_match->root->ops || !best_match->root->ops->find) return Error::NoSys;

    VfsNode* current = best_match->root;
    current->mount = best_match;

    char components[16][32];
    const usize count = split_path(sub_path, components, 16);

    filesystem::dentry* current_dentry = nullptr;

    for (usize i = 0; i < count; i++) {
        filesystem::dentry* next_dentry = nullptr;

        VfsNode* cached_node = filesystem::g_dentry_cache.lookup_component(current_dentry, components[i], &next_dentry);

        if (cached_node) {
            current = cached_node;
            current_dentry = next_dentry;
        } else {
            VfsNode* parent_node = current;
            current = TRY(parent_node->ops->find(parent_node, components[i]));

            current_dentry = filesystem::g_dentry_cache.insert_component(current_dentry, components[i], current);
        }
    }

    filesystem::g_dentry_cache.insert(path, current);

    return Result<VfsNode*>::ok(current);
}

Result<VfsDir*> VFS::opendir(VfsNode* node) {
    if (!node) return Error::Inval;
    if (node->type != VfsNodeType::Directory) return Error::NotDir;
    if (!node->ops || !node->ops->opendir) return Error::NoSys;

    void* handle = TRY(node->ops->opendir(node));

    auto* dir = new VfsDir();
    if (!dir) return Error::NoMem;

    dir->node = node;
    dir->handle = handle;
    return Result<VfsDir*>::ok(dir);
}

Result<bool> VFS::readdir(const VfsDir* dir, dirent_t* out) {
    if (!dir || !dir->node || !dir->node->ops || !dir->node->ops->readdir) return Error::NoSys;
    return dir->node->ops->readdir(dir->handle, out);
}

void VFS::close(VfsNode* node) {
    if (!node || !node->ops || !node->ops->close || node->permanent) return;

    if ((__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_ACQ_REL) == 0)) {
        node->ops->close(node);
    }
}

void VFS::closedir(VfsDir* dir) {
    if (!dir) return;
    if (dir->node && dir->node->ops && dir->node->ops->closedir && dir->handle) dir->node->ops->closedir(dir->handle);
    delete dir;
}

Result<usize> VFS::read(const VfsNode* node, const usize offset, const usize size, void* buffer) {
    if (!node || !node->ops || !node->ops->read) return Error::NoSys;
    return node->ops->read(node, offset, size, buffer);
}

Result<usize> VFS::write(VfsNode* node, const usize offset, const usize size, const void* buffer) {
    if (!node || !node->ops || !node->ops->write) return Error::NoSys;
    if (is_read_only(node)) return Error::RoFs;
    return node->ops->write(node, offset, size, buffer);
}

VoidResult VFS::create(const char* path) {
    if (!path) return Error::Inval;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return Error::NoEnt;

    if (is_read_only(parent)) {
        close(parent);
        return Error::RoFs;
    }
    if (!parent->ops || !parent->ops->create) {
        close(parent);
        return Error::NoSys;
    }

    const auto result = parent->ops->create(parent, name);
    close(parent);

    if (result.is_ok())
        filesystem::g_dentry_cache.invalidate(path);

    return result;
}

VoidResult VFS::rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return Error::Inval;

    VfsNode *old_parent = nullptr, *new_parent = nullptr;
    char old_name[64], new_name[64];

    if (!resolve_parent(old_path, &old_parent, old_name)) return Error::NoEnt;
    if (!resolve_parent(new_path, &new_parent, new_name)) {
        close(old_parent);
        return Error::NoEnt;
    }

    if (is_read_only(old_parent)) {
        close(old_parent);
        close(new_parent);
        return Error::RoFs;
    }

    if (old_parent != new_parent) {
        close(old_parent);
        close(new_parent);
        return Error::XDev;
    }

    if (!old_parent->ops || !old_parent->ops->rename) {
        close(old_parent);
        close(new_parent);
        return Error::NoSys;
    }

    const auto result = old_parent->ops->rename(old_parent, old_name, new_parent, new_name);
    close(old_parent);
    close(new_parent);

    if (result.is_ok()) {
        filesystem::g_dentry_cache.invalidate_prefix(old_path);
        filesystem::g_dentry_cache.invalidate(new_path);
    }

    return result;
}

VoidResult VFS::mkdir(const char* path) {
    if (!path) return Error::Inval;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return Error::NoEnt;

    if (is_read_only(parent)) {
        close(parent);
        return Error::RoFs;
    }
    if (!parent->ops || !parent->ops->mkdir) {
        close(parent);
        return Error::NoSys;
    }

    const auto result = parent->ops->mkdir(parent, name);
    close(parent);

    if (result.is_ok())
        filesystem::g_dentry_cache.invalidate(path);

    return result;
}

VoidResult VFS::rmdir(const char* path) {
    if (!path) return Error::Inval;

    {
        SpinlockGuard guard(mount_points_lock_);
        for (const auto& mp : *mount_points_)
            if (strcmp(mp->path, path) == 0) return Error::Perm;
    }

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return Error::NoEnt;

    if (is_read_only(parent)) {
        close(parent);
        return Error::RoFs;
    }
    if (!parent->ops || !parent->ops->rmdir) {
        close(parent);
        return Error::NoSys;
    }

    const auto result = parent->ops->rmdir(parent, name);
    close(parent);

    if (result.is_ok())
        filesystem::g_dentry_cache.invalidate_prefix(path);

    return result;
}

VoidResult VFS::unlink(const char* path) {
    if (!path) return Error::Inval;

    VfsNode* parent = nullptr;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return Error::NoEnt;

    if (is_read_only(parent)) {
        close(parent);
        return Error::RoFs;
    }
    if (!parent->ops || !parent->ops->unlink) {
        close(parent);
        return Error::NoSys;
    }

    const auto result = parent->ops->unlink(parent, name);
    close(parent);

    if (result.is_ok())
        filesystem::g_dentry_cache.invalidate(path);

    return result;
}

VoidResult VFS::truncate(VfsNode* node, const usize new_size) {
    if (!node || !node->ops || !node->ops->truncate) return Error::NoSys;
    if (is_read_only(node)) return Error::RoFs;
    return node->ops->truncate(node, new_size);
}

VoidResult VFS::stat(const VfsNode* node, struct stat* out) {
    if (!node || !node->ops || !node->ops->stat) return Error::NoSys;
    return node->ops->stat(node, out);
}

VoidResult VFS::chown(VfsNode* node, const u32 uid, const u32 gid) {
    if (!node || !node->ops || !node->ops->chown) return Error::NoSys;
    if (is_read_only(node)) return Error::RoFs;
    return node->ops->chown(node, uid, gid);
}

VoidResult VFS::chmod(VfsNode* node, const u16 mode) {
    if (!node || !node->ops || !node->ops->chmod) return Error::NoSys;
    if (is_read_only(node)) return Error::RoFs;
    return node->ops->chmod(node, mode);
}

bool VFS::is_seekable(const VfsNode* node) {
    if (!node) return false;
    return node->seekable;
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

    filesystem::g_dentry_cache.flush();

    FilesystemDetector::init();
    FilesystemDetector::register_all_drivers();
    FilesystemDetector::scan_and_mount_all();
    // FilesystemDetector::print_detected_filesystems();
}

void VFS::emergency_detach_device(const BlockDevice* device) {
    filesystem::g_dentry_cache.flush();
    FilesystemDetector::emergency_detach_device(device);
}

/*
void VFS::get_stats(VfsStats* stats) { ... }
*/

namespace {
    KernelDevice* resolve_block_device(const char* source) {
        if (!source) return nullptr;

        // Normalize: strip leading '/' and optional "dev/"
        if (source[0] == '/') source++;
        if (strncmp(source, "dev/", 4) == 0) source += 4;

        auto devices = DeviceManager::query([](const KernelDevice* kd) { return kd->block != nullptr; });
        for (auto* kd : devices) {
            if (kd && kd->name && strcmp(kd->name, source) == 0) return kd;
        }
        return nullptr;
    }
} // namespace

i64 VFS::mount(const char* source, const char* target, const char* fstype, const u64 flags) {
    if (!target || target[0] != '/') return -EINVAL;

    if (flags & MS_REMOUNT) {
        MountPoint* mp = find_mount_point(target);
        if (!mp) return -EINVAL;
        mp->flags = flags & ~MS_REMOUNT;
        return 0;
    }

    const KernelDevice* kd = resolve_block_device(source);
    if (!kd) return -ENODEV;

    for (const auto& mp : get_mount_points_snapshot()) {
        if (mp->device && mp->device->device == kd->block) return -EBUSY;
    }

    return FilesystemDetector::mount_manual(kd, target, fstype, flags);
}

i64 VFS::unmount(const char* target) {
    if (!target || target[0] != '/') return -EINVAL;

    MountPoint* mp = find_mount_point(target);
    if (!mp) return -ENOENT;
    if (mp->is_root_device || mp->is_virtual) return -EACCES;

    if (!FilesystemDetector::unmount(mp)) return -EBUSY;

    filesystem::g_dentry_cache.invalidate_prefix(target);

    return 0;
}

void VFS::unmount_all() {
    filesystem::g_dentry_cache.flush();
    FilesystemDetector::unmount_all();
}

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
    return false; // Not found
}
