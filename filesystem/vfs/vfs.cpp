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
#include "../../include/log.h"

static MountPoint mounts[MAX_MOUNTS];
static size_t mount_count = 0;

void vfs_init() {
    mount_count = 0;
}

int vfs_mount(BlockDevice* dev, const char* path, const char* fs_name) {
    if (mount_count >= MAX_MOUNTS) return -1;

    FileSystemDriver* driver = find_fs_driver(fs_name);
    if (!driver) return -2;

    VfsNode* root = driver->mount(dev);
    if (!root) return -3;

    strncpy(mounts[mount_count].path, path, sizeof(mounts[mount_count].path) - 1);
    mounts[mount_count].path[sizeof(mounts[mount_count].path) - 1] = '\0';

    mounts[mount_count].root = root;
    mount_count++;
    return 0;
}

VfsNode* vfs_open(const char* path) {
    if (!path || path[0] != '/') return 0;

    // Kandidat für spätere Pfadkomponenten (z.B. ["mnt", "usb", "file.txt"])
    char components[16][32];
    size_t componentCount = split_path(path, components, 16);
    if (componentCount == 0) return 0;

    // Mountpoint suchen
    for (size_t i = 0; i < mount_count; i++) {
        const char* mpath = mounts[i].path;
        size_t mountLen = strlen(mpath);

        // Prüfen, ob path unter dem Mountpoint liegt
        if (strncmp(path, mpath, mountLen) == 0 &&
            (path[mountLen] == '/' || path[mountLen] == '\0')) {

            VfsNode* current = mounts[i].root;

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

size_t vfs_read(VfsNode* node, size_t offset, size_t size, void* buffer) {
    if (!node || !node->ops || !node->ops->read) return 0;
    return node->ops->read(node, offset, size, buffer);
}

int vfs_readdir(VfsNode* node, size_t index, char* out_name, size_t max_len) {
    if (!node || !node->ops || !node->ops->readdir) return -1;
    return node->ops->readdir(node, index, out_name, max_len);
}

void vfs_close(VfsNode* node) {
    if (!node || !node->ops || !node->ops->close) return;
    node->ops->close(node);
}
