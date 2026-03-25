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

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "../vfs/fs_registry.h"
#include "../vfs/vfs_node.h"
#include "ext4.h"

using namespace ext4;

static VfsNode* make_vfs_node(
    FileSystem* fs, u32 inode, bool is_dir, const char* path, const char* name, const VfsNodeOps* ops
) {
    auto* data = static_cast<Ext4Node*>(kernel::memory::malloc(sizeof(Ext4Node)));
    if (!data) return nullptr;

    data->fs = fs;
    data->inode = inode;
    data->is_dir = is_dir;
    data->file_size = 0;  // TODO: populate via inode_get_size()
    data->entries = nullptr;
    data->entry_count = 0;
    data->current_index = 0;
    snprintf(data->path, sizeof(data->path), "%s", path);

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    if (!node) {
        kernel::memory::free(data);
        return nullptr;
    }

    node->name = name;
    node->type = is_dir ? VfsNodeType::Directory : VfsNodeType::File;
    node->mount = nullptr;
    node->internal_data = data;
    node->permanent = false;
    node->ops = ops;

    return node;
}

static VfsNode* ext4_find(VfsNode* node, const char* name) {
    if (!node) return nullptr;
    const auto* dir = static_cast<Ext4Node*>(node->internal_data);
    if (!dir || !dir->is_dir) return nullptr;

    usize entry_count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, entry_count);
    if (!entries) return nullptr;

    VfsNode* result = nullptr;

    for (usize i = 0; i < entry_count; ++i) {
        if (strcmp(entries[i].get_name(), name) != 0) continue;

        // Build the full path for the child.
        char child_path[512];
        const bool root = (dir->path[0] == '/' && dir->path[1] == '\0');
        snprintf(child_path, sizeof(child_path), "%s%s%s", dir->path, root ? "" : "/", name);

        result = make_vfs_node(
            dir->fs, entries[i].get_inode(), entries[i].is_dir(), child_path, entries[i].get_name(), node->ops
        );
        break;
    }

    kernel::memory::free(entries);
    return result;
}

static void* ext4_opendir(const VfsNode* node) {
    if (!node) return nullptr;
    const auto* dir = static_cast<const Ext4Node*>(node->internal_data);
    if (!dir) return nullptr;

    usize count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, count);
    if (!entries) return nullptr;

    Log::debug("[ext4] opendir: %zu entries in inode=%u", count, dir->inode);

    auto* handle = static_cast<Ext4DirHandle*>(kernel::memory::malloc(sizeof(Ext4DirHandle)));
    if (!handle) {
        kernel::memory::free(entries);
        return nullptr;
    }

    handle->entries = entries;
    handle->count = count;
    handle->index = 0;
    return handle;
}

static int ext4_readdir(void* dir_handle, dirent_t* out) {
    auto* h = static_cast<Ext4DirHandle*>(dir_handle);
    if (!h || h->index >= h->count) return 0;

    const FileEntry& fe = h->entries[h->index++];
    const usize len = strlen(fe.get_name());
    memcpy(out->name, fe.get_name(), len);
    out->name[len] = '\0';

    return 1;
}

static void ext4_closedir(void* dir_handle) {
    auto* h = static_cast<Ext4DirHandle*>(dir_handle);
    if (!h) return;

    kernel::memory::free(h->entries);
    kernel::memory::free(h);
}

static VfsNodeOps ext4_ops = {
    .read = nullptr,
    .write = nullptr,
    .find = ext4_find,
    .close = nullptr,
    .opendir = ext4_opendir,
    .readdir = ext4_readdir,
    .closedir = ext4_closedir,
    .create = nullptr,
    .rename = nullptr,
    .mkdir = nullptr,
    .rmdir = nullptr,
    .unlink = nullptr,
    .ioctl = nullptr,
    .stat = nullptr,
    .truncate = nullptr,
    .poll = nullptr,
};

static int ext4_probe(BlockDevice* dev, FilesystemInfo* fs_info) {
    FileSystem fs(dev);
    if (!fs.is_valid()) return 0;

    constexpr usize label_len = 16;
    memcpy(fs_info->label, fs.get_superblock()->s_volume_name, label_len);
    return 1;
}

static VfsNode* ext4_mount(BlockDevice* dev) {
    auto* fs = new FileSystem(dev);
    if (!fs->is_valid()) {
        delete fs;
        return nullptr;
    }

    VfsNode* root = make_vfs_node(fs, EXT4_ROOT_INODE, true, "/", "/", &ext4_ops);
    if (!root) {
        delete fs;
        return nullptr;
    }

    root->permanent = true;
    return root;
}

static bool ext4_unmount(VfsNode* /*node*/) {
    // TODO: release FileSystem and all cached nodes.
    return true;
}

FileSystemDriver ext4_driver = {
    .name = "ext4",
    .probe = ext4_probe,
    .mount = ext4_mount,
    .unmount = ext4_unmount,
};