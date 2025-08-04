// vfs.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.
#include "vfs_node.h"
#include "../../include/path.h"
#include "../../include/string.h"
#include "vfs.h"
#include "fs_registry.h"
#include "../../kernel/include/errno.h"
#include "vfs_helper.h"

static MountPoint mounts[MAX_MOUNTS];
static size_t mount_count = 0;

void vfs_init() {
    mount_count = 0;
}

int vfs_mount(BlockDevice *dev, const char *path, const char *fs_name) {
    if (mount_count >= MAX_MOUNTS) return -1;

    FileSystemDriver *driver = find_fs_driver(fs_name);
    if (!driver) return -2;

    VfsNode *root = driver->mount(dev);
    if (!root) return -3;

    strncpy(mounts[mount_count].path, path, sizeof(mounts[mount_count].path) - 1);
    mounts[mount_count].path[sizeof(mounts[mount_count].path) - 1] = '\0';

    mounts[mount_count].root = root;
    mount_count++;
    return SUCCESS_CODE;
}

VfsNode *vfs_open(const char *path) {
    if (!path || path[0] != '/') return 0;

    char components[16][32];
    size_t componentCount = split_path(path, components, 16);
    if (componentCount == 0) return 0;

    // Mountpoint suchen
    for (size_t i = 0; i < mount_count; i++) {
        const char *mpath = mounts[i].path;
        size_t mountLen = strlen(mpath);

        // Prüfen, ob path unter dem Mountpoint liegt
        if (strncmp(path, mpath, mountLen) == 0 &&
            (path[mountLen] == '/' || path[mountLen] == '\0')) {
            VfsNode *current = mounts[i].root;

            // z.B. bei /mnt/usb/foo/bar.txt → überspringe die Komponenten ["mnt", "usb"]
            size_t skip = 0;
            char mount_parts[16][32];
            size_t mparts = split_path(mpath, mount_parts, 16);
            skip = mparts;

            for (size_t j = skip; j < componentCount; j++) {
                if (!current || !current->ops || !current->ops->find) return 0;

                current = current->ops->find(current, components[j]);
                if (!current) return 0;
            }

            return current;
        }
    }

    return 0; // Kein Mountpoint gefunden
}

VfsDir *vfs_opendir(const char *path) {
    VfsNode *node = vfs_open(path);
    if (!node || node->type != VfsNodeType::Directory) return nullptr;

    FAT32::Fat32Node *fatNode = (FAT32::Fat32Node *)node->internal_data;
    FAT32::FileEntry *entries = fatNode->fs->ReadDirectory(fatNode->cluster, fatNode->entryCount);
    if (!entries) {
        vfs_close(node);
        return nullptr;
    }

    auto *dir = (VfsDir *)malloc(sizeof(VfsDir));
    dir->node = node;
    dir->currentIndex = 0;
    dir->entries = entries;
    dir->entryCount = fatNode->entryCount;

    return dir;
}

void vfs_closedir(VfsDir *dir) {
    if (!dir) return;

    if (dir->entries) free(dir->entries);
    if (dir->node) vfs_close(dir->node);
    free(dir);
}


size_t vfs_read(VfsNode *node, size_t offset, size_t size, void *buffer) {
    if (!node || !node->ops || !node->ops->read) return 0;
    return node->ops->read(node, offset, size, buffer);
}

int vfs_readdir(VfsDir *dir, char *out_name, size_t max_len) {
    if (!dir || dir->currentIndex >= dir->entryCount) return 0;

    const char *name = dir->entries[dir->currentIndex].GetName();
    if (!name) return 0;

    strncpy(out_name, name, max_len - 1);
    out_name[max_len - 1] = '\0';
    dir->currentIndex++;

    return 1;
}


void vfs_close(VfsNode *node) {
    if (!node || !node->ops || !node->ops->close || node->permanent) return;
    node->ops->close(node);
}

int vfs_rename(const char* oldPath, const char* newPath) {
    if (!oldPath || !newPath) return -EINVAL;

    VfsNode* oldParent;
    VfsNode* newParent;
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
