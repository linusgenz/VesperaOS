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

#include <kernel/scheduling.h>

#include <errno.h>
#include "../../include/kernel/devices/device_manager.h"
#include "fs_detection.h"
#include "../dirent.h"
#include <kernel/realm/realm_manager.h>
#include <log.h>

Vector<MountPoint*>* VFS::mount_points = nullptr;
spinlock_t VFS::mount_points_lock;


void VFS::init()
{
    mount_points = new Vector<MountPoint*>();
    mount_points_lock.init("mount_points_lock");

    FilesystemDetector::Init();

    FilesystemDetector::RegisterAllDrivers();

    // FilesystemDetector::ScanAndMountAll();

    // FilesystemDetector::PrintDetectedFilesystems();

    Log::Info("[VFS] Initialization complete");
}

VfsNode* VFS::mount_virtual(VfsNode* root, const char* mount_path)
{
    if (!root || !mount_path) return nullptr;

    auto* mp = new MountPoint();
    strncpy(mp->path, mount_path, sizeof(mp->path) - 1);
    mp->path[sizeof(mp->path) - 1] = '\0';
    mp->is_virtual = true;
    mp->root = root;

    {
        spinlock_guard g(mount_points_lock);
        mount_points->push_back(mp);
    }

    return root;
}

VfsNode* VFS::open(const char* path)
{
    if (!path || !*path) return nullptr;

    char abs_path[256];

    // Relativer Pfad → prepend current_dir
    if (path[0] != '/')
    {
        const RealmID rid = kernel::scheduling::get_current_unit()->rid;
        const Realm* realm = RealmManager::get(rid);
        if (const char* cwd = realm->cwd_path; strcmp(cwd, "/") == 0)
            snprintf(abs_path, sizeof(abs_path), "/%s", path);
        else
            snprintf(abs_path, sizeof(abs_path), "%s/%s", cwd, path);

        path = abs_path;
    }

    // --- Mountpoint-Suche ---
    MountPoint* best_match = nullptr;
    size_t best_len = 0;

    {
        spinlock_guard g(mount_points_lock);
        for (auto& mp : *mount_points)
        {
            size_t len = strlen(mp->path);

            if (strncmp(path, mp->path, len) == 0 &&
                (strcmp(mp->path, "/") == 0 || path[len] == '/' || path[len] == '\0') &&
                len > best_len)
            {
                best_match = mp;
                best_len = len;
            }
        }
    }

    if (!best_match) return nullptr;

    const char* sub_path = path + best_len;
    if (*sub_path == '/') sub_path++;

    if (!best_match->root->ops || !best_match->root->ops->find)
        return nullptr;

    VfsNode* current = best_match->root;

    char components[16][32];
    size_t count = split_path(sub_path, components, 16);

    for (size_t i = 0; i < count; i++)
    {
        current = current->ops->find(current, components[i]);
        if (!current) return nullptr;
    }

    return current;
}

VfsDir* VFS::opendir(const char* path)
{
    VfsNode* node = open(path);
    if (!node || node->type != VfsNodeType::Directory) return nullptr;
    if (!node->ops || !node->ops->opendir) return nullptr;

    void* handle = node->ops->opendir(node);
    if (!handle)
    {
        close(node);
        return nullptr;
    }

    auto* dir = static_cast<VfsDir*>(malloc(sizeof(VfsDir)));
    dir->node = node;
    dir->handle = handle;
    return dir;
}


int VFS::readdir(const VfsDir* dir, dirent_t* out)
{
    if (!dir || !dir->node || !dir->node->ops || !dir->node->ops->readdir)
        return 0;
    return dir->node->ops->readdir(dir->handle, out);
}

void VFS::close(VfsNode* node)
{
    if (!node || !node->ops || !node->ops->close || node->permanent) return;
    node->ops->close(node);
}

void VFS::closedir(VfsDir* dir)
{
    if (!dir) return;
    if (dir->node && dir->node->ops && dir->node->ops->closedir && dir->handle)
    {
        dir->node->ops->closedir(dir->handle);
    }
    if (dir->node) close(dir->node);
    free(dir);
}

size_t VFS::read(const VfsNode* node, size_t offset, size_t size, void* buffer)
{
    if (!node || !node->ops || !node->ops->read) return 0;
    return node->ops->read(node, offset, size, buffer);
}

int VFS::create(const char* path)
{
    if (!path) return -EINVAL;

    VfsNode* parent;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (!parent->ops || !parent->ops->create)
    {
        close(parent);
        return -ENOSYS;
    }

    int result = parent->ops->create(parent, name);
    close(parent);
    return result;
}

int VFS::rename(const char* oldPath, const char* newPath)
{
    if (!oldPath || !newPath) return -EINVAL;

    VfsNode *oldParent, *newParent;
    char oldName[64], newName[64];

    if (!resolve_parent(oldPath, &oldParent, oldName)) return -ENOENT;
    if (!resolve_parent(newPath, &newParent, newName))
    {
        close(oldParent);
        return -ENOENT;
    }

    if (oldParent != newParent)
    {
        close(oldParent);
        close(newParent);
        return -EXDEV;
    }

    if (!oldParent->ops || !oldParent->ops->rename)
    {
        close(oldParent);
        close(newParent);
        return -ENOSYS;
    }

    int status = oldParent->ops->rename(oldParent, oldName, newName);
    close(oldParent);
    close(newParent);
    return status;
}

int VFS::mkdir(const char* path)
{
    if (!path) return -EINVAL;

    VfsNode* parent;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (!parent->ops || !parent->ops->mkdir)
    {
        close(parent);
        return -ENOSYS;
    }

    int result = parent->ops->mkdir(parent, name);
    close(parent);
    return result;
}

int VFS::rmdir(const char* path)
{
    if (!path) return -EINVAL;

    {
        spinlock_guard guard(mount_points_lock);
        for (const auto& mp : *mount_points)
        {
            if (strcmp(mp->path, path) == 0) return -EPERM;
        }
    }

    VfsNode* parent;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (!parent->ops || !parent->ops->rmdir)
    {
        close(parent);
        return -ENOSYS;
    }

    int result = parent->ops->rmdir(parent, name);
    close(parent);
    return result;
}

int VFS::unlink(const char* path)
{
    if (!path) return -EINVAL;

    VfsNode* parent;
    char name[64];
    if (!resolve_parent(path, &parent, name)) return -ENOENT;

    if (!parent->ops || !parent->ops->unlink)
    {
        close(parent);
        return -ENOSYS;
    }

    int result = parent->ops->unlink(parent, name);
    close(parent);
    return result;
}

bool VFS::probe_filesystem(BlockDevice* device)
{
    FilesystemInfo info{};
    return FilesystemDetector::DetectFilesystem(device, &info);
}

void VFS::list_devices()
{
    FilesystemDetector::PrintDetectedFilesystems();
}

void VFS::remount_all()
{
    Log::Info("[VFS] Remounting all detected devices...");
    FilesystemDetector::Init();
    FilesystemDetector::RegisterAllDrivers();
    FilesystemDetector::ScanAndMountAll();
    FilesystemDetector::PrintDetectedFilesystems();
}

void VFS::get_stats(VfsStats* stats)
{
    if (!stats) return;

    stats->total_devices = DeviceManager::GetDeviceCount();
    stats->mounted_devices = 0;
    stats->supported_filesystems = 0;

    auto devices = DeviceManager::GetDevices();
    for (size_t i = 0; i < stats->total_devices; i++)
    {
        FilesystemInfo info;
        if (FilesystemDetector::DetectFilesystem(devices[i], &info) && info.mounted)
        {
            stats->mounted_devices++;
        }
    }

    // Count supported filesystem types
    // For now, just count the registered drivers
    stats->supported_filesystems = 1; // FAT32 is always supported
    // TODO: Add count of other registered drivers when implemented
}

void VFS::add_mount_point(MountPoint* mp)
{
    spinlock_guard g(mount_points_lock);
    mount_points->push_back(mp);
}

size_t VFS::mount_points_count()
{
    spinlock_guard g(mount_points_lock);
    return mount_points->size();
}

MountPoint* VFS::find_mount_point(const char* path)
{
    if (!path) return nullptr;

    spinlock_guard g(mount_points_lock);

    for (auto& mp : *mount_points)
    {
        if (strcmp(mp->path, path) == 0)
        {
            return mp;
        }
    }

    return nullptr; // not found
}


bool VFS::remove_mount_point(MountPoint* mp)
{
    if (!mp) return false;
    spinlock_guard g(mount_points_lock);

    for (size_t i = 0; i < mount_points->size(); ++i)
    {
        MountPoint* mp_iter = (*mount_points)[i];

        if (mp_iter == mp)
        {
            if (mp_iter->device)
            {
                delete mp_iter->device;
                mp_iter->device = nullptr;
            }
            mount_points->erase(i);
            return true;
        }
    }
    return false; // Not found
}
