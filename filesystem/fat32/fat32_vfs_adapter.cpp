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

#include "../vfs/fs_registry.h"
#include "fat32_vfs_adapter.h"
#include "fat32.h"
#include "../../include/log.h"
#include "../../kernel/include/errno.h"

using namespace FAT32;


bool fat32_resolve_path(FAT32::FileSystem *fs, const char *path, Fat32Node *outNode) {
    uint32_t cluster = fs->ResolvePathToCluster(path);
    if (cluster == 0) return false;

    strncpy(outNode->path, path, strlen(path));
    outNode->cluster = cluster;
    outNode->fs = fs;
    outNode->isDir = fs->IsDir(cluster);
    return true;
}

static size_t fat32_read(VfsNode *node, size_t offset, size_t size, void *buffer) {
    Fat32Node *fnode = (Fat32Node *) node->internal_data;
    if (!fnode || !buffer || size == 0) return 0;

    char *temp = (char *) kernel::memory::malloc(size);
    if (!temp) return 0;

    size_t actual = 0;

    bool ok = fnode->fs->ReadFile(fnode, temp, size, actual);

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
            childData->fileSize = entries[i].GetFileSize();

            // neuen Pfad bauen: "/EFI/BOOT" + "/" + "foo.txt"
            snprintf(childData->path, sizeof(childData->path),
                     "%s%s%s",
                     dir->path,
                     strcmp(dir->path, "/") == 0 ? "" : "/",
                     name);

            childData->cluster = entries[i].GetFirstCluster();

            VfsNode *child = (VfsNode *) malloc(sizeof(VfsNode));
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

void* fat32_opendir(VfsNode* dir) {
    auto* fatNode = (Fat32Node*)dir->internal_data;
    auto* handle = new Fat32DirHandle();
    handle->entries = fatNode->fs->ReadDirectory(fatNode->cluster, handle->count);
    handle->index = 0;
    return handle;
}

int fat32_readdir(void* h, char* out_name, size_t max_len) {
    auto* handle = (Fat32DirHandle*)h;
    if (!handle || handle->index >= handle->count) return 0;

    const char* name = handle->entries[handle->index].GetName();
    if (!name) return 0;

    strncpy(out_name, name, max_len - 1);
    out_name[max_len - 1] = '\0';

    handle->index++;
    return 1;
}

void fat32_closedir(void* h) {
    auto* handle = (Fat32DirHandle*)h;
    if (!handle) return;

    if (handle->entries) {
        free(handle->entries);
    }
    delete handle;
}
/*
static int fat32_readdir(VfsNode *node, char *out_name, size_t max_len) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    if (!dir || !dir->isDir) return -1;

    // Lazy load
    if (!dir->entries) {
        dir->entries = dir->fs->ReadDirectory(dir->path, dir->entryCount);
        dir->currentIndex = 0;
    }

    if (!dir->entries || dir->currentIndex >= dir->entryCount)
        return 0;

    const char *name = dir->entries[dir->currentIndex].GetName();
    strncpy(out_name, name, max_len - 1);
    out_name[max_len - 1] = '\0';

    dir->currentIndex++;
    return 1;
}
*/

static void fat32_close(VfsNode *node) {
    if (!node) return;

    Fat32Node *data = (Fat32Node *) node->internal_data;
    if (data) {
        if (data->entries) kernel::memory::free(data->entries);
        kernel::memory::free(data);
    }

    kernel::memory::free(node);
}

static int fat32_create(VfsNode *node, const char *name) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    return dir->fs->CreateFile(dir, name) ? 0 : -1;
}

static int fat32_rename(VfsNode *node, const char *oldName, const char *newName) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    if (!dir || !oldName || !newName) return -EINVAL;

    if (!dir->fs->Rename(dir, oldName, newName)) {
        return -EIO; // Could not rename entry
    }

    return 0;
}

static int fat32_mkdir(VfsNode *node, const char *name) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    return dir->fs->CreateDirectory(dir, name) ? 0 : -1;
}

static int fat32_rmdir(VfsNode *node, const char *name) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    return dir->fs->RemoveDirectory(dir, name) ? 0 : -1;
}

static int fat32_unlink(VfsNode *node, const char *name) {
    Fat32Node *dir = (Fat32Node *) node->internal_data;
    return dir->fs->DeleteFile(dir, name) ? 0 : -1;
}

static size_t fat32_file_size(VfsNode *node) {
    Fat32Node *file = (Fat32Node *) node->internal_data;
    return file->fileSize;
}

static VfsNodeOps fat32_ops = {
    .read = fat32_read,
    .write = nullptr, // TODO
    .find = fat32_find,
    .close = fat32_close,
    .file_size = fat32_file_size,
    .opendir = fat32_opendir,
    .readdir = fat32_readdir,
    .closedir = fat32_closedir,
    .create = fat32_create,
    .rename = fat32_rename,
    .mkdir = fat32_mkdir,
    .rmdir = fat32_rmdir,
    .unlink = fat32_unlink
};

VfsNode *wrap_fat32_root(FileSystem *fs) {
    if (!fs) return nullptr;

    Fat32Node *root = (Fat32Node *) kernel::memory::malloc(sizeof(Fat32Node));
    root->fs = fs;
    root->isDir = true;
    root->path[0] = '/';
    root->path[1] = '\0';
    root->cluster = fs->GetRootCluster();

    VfsNode *node = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    node->name = "/";
    node->type = VfsNodeType::Directory;
    node->internal_data = root;
    node->permanent = true;
    node->ops = &fat32_ops;

    return node;
}

int fat32_probe(BlockDevice *dev) {
    const FileSystem fs(dev);
    return fs.is_valid();
}

VfsNode *fat32_mount(BlockDevice *dev) {
    auto *fs = new FileSystem(dev);
    Log::debug("fat32_mount valid? : %u", fs->is_valid());
    if (!fs->is_valid()) return nullptr;
    return wrap_fat32_root(fs);
}

FileSystemDriver fat32_driver = {
    .name = "fat32",
    .probe = fat32_probe,
    .mount = fat32_mount
};
