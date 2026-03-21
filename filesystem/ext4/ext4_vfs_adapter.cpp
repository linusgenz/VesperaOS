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

#include "../vfs/fs_registry.h"
#include "../vfs/vfs_node.h"
#include "ext4.h"
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

using namespace ext4;

int ext4_probe(BlockDevice* dev, FilesystemInfo* fs_info)
{
    return 0;
    FileSystem fs(dev);

    usize len = 16;
    memcpy(fs_info->label, fs.get_superblock()->s_volume_name, len);

    return fs.is_valid();
}

static VfsNode* ext4_find(VfsNode* node, const char* name)
{
    auto* dir = static_cast<Ext4Node*>(node->internal_data);
    if (!dir || !dir->is_dir) return nullptr;

    usize entry_count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, entry_count);
    if (!entries) return nullptr;

    for (usize i = 0; i < entry_count; i++)
    {
        if (const char* entry_name = entries[i].get_name(); strcmp(entry_name, name) == 0)
        {
            auto* child_data = static_cast<Ext4Node*>(kernel::memory::malloc(sizeof(Ext4Node)));
            if (!child_data)
            {
                kernel::memory::free(entries);
                return nullptr;
            }

            child_data->fs = dir->fs;
            child_data->inode = entries[i].get_inode();
            child_data->is_dir = entries[i].is_dir();
            child_data->file_size = 0; // TODO  inode_get_size()

            // construct path
            snprintf(child_data->path, sizeof(child_data->path),
                     "%s%s%s",
                     dir->path,
                     strcmp(dir->path, "/") == 0 ? "" : "/",
                     name);

            auto* child = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
            if (!child)
            {
                kernel::memory::free(child_data);
                kernel::memory::free(entries);
                return nullptr;
            }

            child->name = entries[i].get_name();
            child->type = child_data->is_dir ? VfsNodeType::Directory : VfsNodeType::File;
            child->internal_data = child_data;
            child->ops = node->ops;

            kernel::memory::free(entries);
            return child;
        }
    }

    kernel::memory::free(entries);
    return nullptr;
}


void* ext4_opendir(const VfsNode* dir)
{
    const auto* node = static_cast<Ext4Node*>(dir->internal_data);
    if (!node) return nullptr;

    usize count = 0;
    FileEntry* entries = node->fs->read_directory(node->inode, count);
    Log::debug("dir->fs->read_directory: %d entries", count);
    if (!entries) return nullptr;

    auto* handle = static_cast<Ext4DirHandle*>(kernel::memory::malloc(sizeof(Ext4DirHandle)));
    handle->entries = entries;
    handle->count = count;
    handle->index = 0;
    return handle;
}

int ext4_readdir(void* dir_handle, dirent_t* out)
{
    if (const auto* h = static_cast<Ext4DirHandle*>(dir_handle); !h || h->index >= h->count) return 0;
    /*
        FileEntry& fe = h->entries[h->index++];
        usize len = strlen(fe.GetName());
        memcpy(entry, fe.GetName(), len);
        entry[len] = '\0';
    */
    return 1;
}

void ext4_closedir(void* dir_handle)
{
    auto* h = static_cast<Ext4DirHandle*>(dir_handle);
    if (!h) return;
    kernel::memory::free(h->entries);
    kernel::memory::free(h);
}

static void ext_volume_name(const VfsNode* node, char* out, int out_size)
{
    auto* fat_node = static_cast<Ext4Node*>(node->internal_data);
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
    .unlink = nullptr,
    .ioctl = nullptr,
    .stat = nullptr,
    .truncate = nullptr,
    .poll = nullptr,
};

VfsNode* wrap_ext4_root(FileSystem* fs)
{
    if (!fs) return nullptr;

    auto* root = static_cast<Ext4Node*>(kernel::memory::malloc(sizeof(Ext4Node)));
    root->fs = fs;
    root->is_dir = true;
    root->inode = 2; // Root Inode EXT
    root->path[0] = '/';
    root->path[1] = '\0';
    root->file_size = 0;
    root->entries = nullptr;
    root->entry_count = 0;
    root->current_index = 0;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = "/";
    node->type = VfsNodeType::Directory;
    node->mount = nullptr;
    node->internal_data = root;
    node->permanent = true;
    node->ops = &ext4_ops; // TODO

    return node;
}


bool ext4_unmount(VfsNode* node)
{
    return true;
}


VfsNode* ext4_mount(BlockDevice* dev)
{
    return nullptr;
    auto* fs = new FileSystem(dev);
    if (!fs->is_valid())
    {
        delete fs;
        return nullptr;
    }
    return wrap_ext4_root(fs);
}

FileSystemDriver ext4_driver = {
    .name = "ext4",
    .probe = ext4_probe,
    .mount = ext4_mount,
    .unmount = ext4_unmount,
};
