// ext4_vfs_adapter.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.08.25.
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

#include "ext4_vfs_adapter.h"
#include <log.h>
#include <string.h>
#include <kernel/memory.h>

#include "ext4.h"
#include "../vfs/fs_registry.h"
#include "../vfs/vfs_node.h"

using namespace EXT4;

int ext4_probe(BlockDevice *dev) {
    FileSystem fs(dev);
    return fs.is_valid();
}

static VfsNode* ext4_find(const VfsNode* node, const char* name) {
    auto* dir = static_cast<Ext4Node*>(node->internal_data);
    if (!dir || !dir->isDir) return nullptr;

    size_t entryCount = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, entryCount);
    if (!entries) return nullptr;

    for (size_t i = 0; i < entryCount; i++) {
        if (const char* entryName = entries[i].GetName(); strcmp(entryName, name) == 0) {
            auto* childData = static_cast<Ext4Node*>(kernel::memory::malloc(sizeof(Ext4Node)));
            if (!childData) {
                free(entries);
                return nullptr;
            }

            childData->fs = dir->fs;
            childData->inode = entries[i].GetInode();
            childData->isDir = entries[i].isDir();
            childData->fileSize = 0; // TODO  inode_get_size()

            // construct path
            snprintf(childData->path, sizeof(childData->path),
                     "%s%s%s",
                     dir->path,
                     strcmp(dir->path, "/") == 0 ? "" : "/",
                     name);

            auto* child = static_cast<VfsNode*>(malloc(sizeof(VfsNode)));
            if (!child) {
                kernel::memory::free(childData);
                free(entries);
                return nullptr;
            }

            child->name = entries[i].GetName();
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


void* ext4_opendir(const VfsNode* dir) {
    const auto* node = static_cast<Ext4Node*>(dir->internal_data);
    if (!node) return nullptr;

    size_t count = 0;
    FileEntry* entries = node->fs->read_directory(node->inode, count);
    Log::debug("dir->fs->read_directory: %d entries", count);
    if (!entries) return nullptr;

    auto* handle = static_cast<Ext4DirHandle*>(malloc(sizeof(Ext4DirHandle)));
    handle->entries = entries;
    handle->count = count;
    handle->index = 0;
    return handle;
}

int ext4_readdir(void *dir_handle, dirent_t *out) {
    if (const auto* h = static_cast<Ext4DirHandle*>(dir_handle); !h || h->index >= h->count) return 0;
/*
    FileEntry& fe = h->entries[h->index++];
    size_t len = strlen(fe.GetName());
    memcpy(entry, fe.GetName(), len);
    entry[len] = '\0';
*/
    return 1;
}

void ext4_closedir(void* dir_handle) {
    auto* h = reinterpret_cast<Ext4DirHandle*>(dir_handle);
    if (!h) return;
    free(h->entries);
    free(h);
}

static VfsNodeOps ext4_ops = {
    .read = nullptr,
    .write = nullptr, // TODO
    .find = ext4_find,
    .close = nullptr,
    .opendir = ext4_opendir,
    .readdir = ext4_readdir,
    .closedir = ext4_closedir,
    .create = nullptr,
    .rename = nullptr,
    .mkdir = nullptr,
    .rmdir = nullptr,
    .unlink = nullptr
};

VfsNode* wrap_ext4_root(FileSystem *fs) {
    if (!fs) return nullptr;

    auto* root = static_cast<Ext4Node*>(kernel::memory::malloc(sizeof(Ext4Node)));
    root->fs = fs;
    root->isDir = true;
    root->inode = 2; // Root Inode EXT
    root->path[0] = '/';
    root->path[1] = '\0';
    root->fileSize = 0;
    root->entries = nullptr;
    root->entryCount = 0;
    root->currentIndex = 0;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = "/";
    node->type = VfsNodeType::Directory;
    node->internal_data = root;
    node->permanent = true;
    node->ops = &ext4_ops; // TODO

    return node;
}


VfsNode *ext4_mount(BlockDevice *dev) {
    auto *fs = new FileSystem(dev);
    Log::debug("ext4_mount valid? : %u", fs->is_valid());
    if (!fs->is_valid()) {
        delete fs;
return nullptr;
}
    return wrap_ext4_root(fs);
}

FileSystemDriver ext4_driver = {
    .name = "ext4",
    .probe = ext4_probe,
    .mount = ext4_mount
};

