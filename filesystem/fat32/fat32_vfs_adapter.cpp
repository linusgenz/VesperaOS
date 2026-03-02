// fat32_vfs_adapter.cpp
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

#include "fat32_vfs_adapter.h"

#include <kernel/memory.h>

#include "../../include/log.h"
#include "../../kernel/types/types.h"
#include "../vfs/fs_registry.h"
#include "fat32.h"
#include "fat32_lfn.h"
#include "vespera_errno.h"

using namespace FAT32;

static ssize_t fat32_read(const VfsNode* node, size_t offset, size_t size, void* buffer)
{
    if (!node || !buffer) return -EFAULT;
    if (size == 0) return 0;

    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EBADH;

    size_t actual = 0;
    if (!fnode->fs->ReadFile(fnode, buffer, size, actual, offset))
        return -EIO;

    // Offset can be >= actual → EOF
    if (offset >= actual) return 0;

    size_t copySize = actual - offset;
    if (copySize > size) copySize = size;

    return static_cast<ssize_t>(copySize);
}

static ssize_t fat32_write(VfsNode* node, const size_t offset, const size_t size, const void* buffer)
{
    if (!node || !buffer) return -EFAULT;
    if (size == 0) return 0;

    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EBADH;

    if (offset > fnode->fileSize)
    {
        Log::debug("fat32_write: offset beyond file size (hole not supported)");
        return -EINVAL;
    }

    if (!fnode->fs->WriteFile(fnode, buffer, size, offset))
        return -EIO;

    node->size = fnode->fileSize;
    return static_cast<ssize_t>(size);
}

static VfsNode* fat32_find(const VfsNode* node, const char* name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    if (!dir || !dir->isDir) return nullptr;

    size_t entryCount = 0;
    FileEntry* entries = dir->fs->ReadDirectory(dir->path, entryCount);
    if (!entries) return nullptr;

    for (size_t i = 0; i < entryCount; i++)
    {
        if (const char* entryName = entries[i].GetName(); strcmp(entryName, name) == 0)
        {
            auto* childData = static_cast<Fat32Node*>(kernel::memory::malloc(sizeof(Fat32Node)));
            memset(childData, 0, sizeof(Fat32Node));
            if (!childData)
            {
                kernel::memory::free(entries);
                return nullptr;
            }

            childData->fs = dir->fs;
            childData->currentIndex = entries[i].GetIndexInCluster();
            childData->entryCount = entryCount;
            childData->parentCluster = dir->cluster;
            childData->isDir = entries[i].isDir();
            childData->fileSize = entries[i].GetFileSize();
            childData->dirEntry = entries[i].GetDirectoryEntry();
            childData->firstLFNIndex = FindFirstLFNIndex(entries, i);

            // neuen Pfad bauen: "/EFI/BOOT" + "/" + "foo.txt"
            snprintf(childData->path, sizeof(childData->path),
                     "%s%s%s",
                     dir->path,
                     strcmp(dir->path, "/") == 0 ? "" : "/",
                     name);

            childData->cluster = entries[i].GetFirstCluster();

            auto* child = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
            child->name = entries[i].GetName();
            child->type = childData->isDir ? VfsNodeType::Directory : VfsNodeType::File;
            child->internal_data = childData;
            child->ops = node->ops;
            child->size = entries[i].GetFileSize();

            kernel::memory::free(entries);
            return child;
        }
    }

    kernel::memory::free(entries);
    return nullptr;
}

void* fat32_opendir(const VfsNode* dir)
{
    auto* fatNode = static_cast<Fat32Node*>(dir->internal_data);
    auto* handle = new Fat32DirHandle();
    handle->entries = fatNode->fs->ReadDirectory(fatNode->cluster, handle->count);
    handle->index = 0;
    return handle;
}

int fat32_readdir(void* h, dirent_t* out)
{
    auto* handle = static_cast<Fat32DirHandle*>(h);
    if (!handle || handle->index >= handle->count) return 0;

    auto& entry = handle->entries[handle->index];
    const char* name = entry.GetName();
    if (!name) return 0;

    strncpy(out->name, name, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';

    // FAT32 attribute byte
    if (entry.isDir())
    {
        out->type = DT_DIR;
    } /*else if (attr & 0x08) {
        out->type = DT_EXEC; // optional: Volume Label / System
    }*/ else
    {
        out->type = DT_FILE;
    }

    handle->index++;
    return 1;
}

void fat32_closedir(void* h)
{
    auto* handle = static_cast<Fat32DirHandle*>(h);
    if (!handle) return;

    if (handle->entries)
    {
        kernel::memory::free(handle->entries);
    }
    delete handle;
}

static void fat32_close(VfsNode* node)
{
    if (!node) return;

    if (auto* data = static_cast<Fat32Node*>(node->internal_data))
    {
        kernel::memory::free(data);
    }

    kernel::memory::free(node);
}

static int fat32_create(const VfsNode* node, const char* name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->CreateFile(dir, name) ? 0 : -1;
}

static int fat32_rename(const VfsNode* node, const char* old_name, const char* new_name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    if (!dir || !old_name || !new_name) return -EINVAL;

    if (!dir->fs->Rename(dir, old_name, new_name))
    {
        return -EIO; // Could not rename entry
    }

    return 0;
}

static int fat32_mkdir(const VfsNode* node, const char* name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->CreateDirectory(dir, name) ? 0 : -1;
}

static int fat32_rmdir(const VfsNode* node, const char* name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->RemoveDirectory(dir, name) ? 0 : -1;
}

static int fat32_unlink(const VfsNode* node, const char* name)
{
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->DeleteFile(dir, name) ? 0 : -1;
}

static VfsNodeOps fat32_ops = {
    .read = fat32_read,
    .write = fat32_write,
    .find = fat32_find,
    .close = fat32_close,
    .opendir = fat32_opendir,
    .readdir = fat32_readdir,
    .closedir = fat32_closedir,
    .create = fat32_create,
    .rename = fat32_rename,
    .mkdir = fat32_mkdir,
    .rmdir = fat32_rmdir,
    .unlink = fat32_unlink,
    .ioctl = nullptr,
};

VfsNode* wrap_fat32_root(FileSystem* fs)
{
    if (!fs) return nullptr;

    auto* root = static_cast<Fat32Node*>(kernel::memory::malloc(sizeof(Fat32Node)));
    root->fs = fs;
    root->isDir = true;
    root->path[0] = '/';
    root->path[1] = '\0';
    root->cluster = fs->GetRootCluster();

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = "/";
    node->type = VfsNodeType::Directory;
    node->internal_data = root;
    node->permanent = true;
    node->ops = &fat32_ops;

    return node;
}

int fat32_probe(BlockDevice* dev, FilesystemInfo *fs_info)
{
    FileSystem fs(dev);

    size_t len = 11;
    memcpy(fs_info->label, fs.GetBpb()->volumeLabel, len);
    fs_info->label[len] = '\0';

    return fs.is_valid();
}

VfsNode* fat32_mount(BlockDevice* dev)
{
    auto* fs = new FileSystem(dev);
    if (!fs->is_valid()) return nullptr;
    return wrap_fat32_root(fs);
}

bool fat32_unmount(VfsNode* root)
{
    if (!root) return false;

    auto* fatnode = static_cast<Fat32Node*>(root->internal_data);
    if (!fatnode) return false;

    const FileSystem* fs = fatnode->fs;
    if (!fs) return false;

    delete fs;

    kernel::memory::free(fatnode);

    kernel::memory::free(root);

    return true;
}

FileSystemDriver fat32_driver = {
    .name = "fat32",
    .probe = fat32_probe,
    .mount = fat32_mount,
    .unmount = fat32_unmount
};
