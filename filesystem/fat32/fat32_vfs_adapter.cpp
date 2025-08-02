// fat32_vfs_adapter.cpp
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

#include "../vfs/fs_registry.h"
#include "fat32.h"
#include "../../include/log.h"

using namespace FAT32;

struct Fat32Node {
    FAT32::FileSystem *fs;
    char path[256]; // absoluter Pfad, z. B. "/EFI/BOOT"
    bool isDir;
};

static size_t fat32_read(VfsNode *node, size_t offset, size_t size, void *buffer) {
    Fat32Node *fnode = (Fat32Node *) node->internal_data;
    if (!fnode || !buffer || size == 0) return 0;

    char *temp = (char *) kernel::memory::malloc(size);
    if (!temp) return 0;

    size_t actual = 0;

    bool ok = fnode->fs->ReadFile(fnode->path, temp, size, actual);

    if (!ok) {
        kernel::memory::free(temp);
        return 0;
    }

    if (offset >= actual) {
        kernel::memory::free(temp);
        return 0;
    }

    size_t copySize = actual - offset;
    if (copySize > size) copySize = size;

    memcpy(buffer, temp + offset, copySize);
    kernel::memory::free(temp);
    return copySize;
}


static VfsNode *fat32_find(VfsNode *node, const char *name) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    if (!dir || !dir->isDir) return nullptr;

    size_t entryCount = 0;
    FAT32::FileEntry *entries = dir->fs->ReadDirectory(dir->path, entryCount);
    if (!entries) return nullptr;

    for (size_t i = 0; i < entryCount; i++) {
        const char *entryName = entries[i].GetName();
        if (strcmp(entryName, name) == 0) {
            Fat32Node *childData = (Fat32Node *) kernel::memory::malloc(sizeof(Fat32Node));
            if (!childData) {
                kernel::memory::free(entries);
                return nullptr;
            }

            childData->fs = dir->fs;
            childData->isDir = entries[i].isDir();

            // neuen Pfad bauen: "/EFI/BOOT" + "/" + "foo.txt"
            snprintf(childData->path, sizeof(childData->path), "%s/%s", dir->path, name);

            VfsNode *child = (VfsNode *) malloc(sizeof(VfsNode));
            child->name = entries[i].GetName(); // Achtung: ggf. strdup()
            child->type = childData->isDir ? VfsNodeType::Directory : VfsNodeType::File;
            child->internal_data = childData;
            child->ops = node->ops;

            free(entries);
            return child;
        }
    }

    free(entries);
    return nullptr;
}

static int fat32_readdir(VfsNode *node, size_t index, char *out_name, size_t max_len) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    if (!dir || !dir->isDir) return -1;

    size_t entryCount = 0;
    FAT32::FileEntry *entries = dir->fs->ReadDirectory(dir->path, entryCount);
    if (!entries || index >= entryCount) {
        if (entries) kernel::memory::free(entries);
        return 0;
    }

    const char *name = entries[index].GetName();
    strncpy(out_name, name, max_len - 1);
    out_name[max_len - 1] = '\0';

    kernel::memory::free(entries);
    return 1;
}


static void fat32_close(VfsNode *node) {
    if (node) {
        if (node->internal_data) kernel::memory::free(node->internal_data);
        kernel::memory::free(node);
    }
}


static VfsNodeOps fat32_ops = {
    .read = fat32_read,
    .write = nullptr, // TODO
    .find = fat32_find,
    .readdir = fat32_readdir,
    .close = fat32_close
};

VfsNode *wrap_fat32_root(FileSystem *fs) {
    if (!fs) return nullptr;

    Fat32Node *root = (Fat32Node *) kernel::memory::malloc(sizeof(Fat32Node));
    root->fs = fs;
    root->isDir = true;
    root->path[0] = '/';
    root->path[1] = '\0';

    VfsNode *node = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    node->name = "/";
    node->type = VfsNodeType::Directory;
    node->internal_data = root;
    node->ops = &fat32_ops;

    return node;
}

// Adapter-Registrierung
static int fat32_probe(BlockDevice *dev) {
    FileSystem fs(dev);
    return fs.is_valid();
}

static VfsNode *fat32_mount(BlockDevice *dev) {
    FileSystem *fs = new FileSystem(dev); // oder malloc + placement new
    if (!fs->is_valid()) return nullptr;
    return wrap_fat32_root(fs);
}

FileSystemDriver fat32_driver = {
    .name = "fat32",
    .probe = fat32_probe,
    .mount = fat32_mount
};
