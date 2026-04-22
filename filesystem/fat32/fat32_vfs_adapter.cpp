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

#include <uapi/vespera/mount.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "../../include/vespera/types.h"
#include "../vfs/fs_registry.h"
#include "fat32.h"
#include "fat32_lfn.h"
#include "fat32_time.h"
#include "vespera/devices/device_manager.h"
#include "vespera/devices/kernel_device.h"
#include "vespera_errno.h"

using namespace fat32;

static isize fat32_read(const VfsNode* node, const usize offset, const usize size, void* buffer) {
    if (!node || !buffer) return -EFAULT;
    if (size == 0) return 0;

    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EBADH;

    const bool update_atime = !node->mount || !(node->mount->flags & MS_NOATIME);

    usize actual = 0;
    if (!fnode->fs->read_file(fnode, buffer, size, actual, offset, update_atime)) return -EIO;

    return static_cast<isize>(actual);
}

static isize fat32_write(VfsNode* node, const usize offset, const usize size, const void* buffer) {
    if (!node || !buffer) return -EFAULT;
    if (size == 0) return 0;

    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EBADH;

    if (offset > fnode->file_size) {
        Log::debug("fat32_write: offset beyond file size (hole not supported)");
        return -EINVAL;
    }

    if (!fnode->fs->write_file(fnode, buffer, size, offset)) return -EIO;

    node->size = fnode->file_size;
    return static_cast<isize>(size);
}

static VfsNode* fat32_find(VfsNode* node, const char* name) {
    if (!node) return nullptr;
    auto* dir = static_cast<Fat32Node*>(node->internal_data);
    if (!dir || !dir->is_dir) return nullptr;

    usize entry_count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->path, entry_count);
    if (!entries) return nullptr;

    for (usize i = 0; i < entry_count; i++) {
        if (const char* entry_name = entries[i].get_name(); strcmp(entry_name, name) == 0) {
            auto* child_data = new Fat32Node();
            if (!child_data) {
                kernel::memory::free(entries);
                return nullptr;
            }

            child_data->fs = dir->fs;
            child_data->current_index = entries[i].get_index_in_cluster();
            child_data->entry_count = entry_count;
            child_data->parent_cluster = dir->cluster;
            child_data->is_dir = entries[i].is_dir();
            child_data->file_size = entries[i].get_file_size();
            child_data->dir_entry = entries[i].get_directory_entry();
            child_data->first_lfn_index = find_first_lfn_index(entries, i);

            // neuen Pfad bauen: "/EFI/BOOT" + "/" + "foo.txt"
            snprintf(
                child_data->path,
                sizeof(child_data->path),
                "%s%s%s",
                dir->path,
                strcmp(dir->path, "/") == 0 ? "" : "/",
                name
            );

            child_data->cluster = entries[i].get_first_cluster();

            auto* child = new VfsNode();
            child->name = strdup(entries[i].get_name());
            child->type = child_data->is_dir ? VfsNodeType::Directory : VfsNodeType::File;
            child->mount = node->mount;
            child->internal_data = child_data;
            child->ops = node->ops;
            child->size = entries[i].get_file_size();

            kernel::memory::free(entries);
            return child;
        }
    }

    kernel::memory::free(entries);
    return nullptr;
}

void* fat32_opendir(const VfsNode* dir) {
    if (!dir) return nullptr;
    const auto* fat_node = static_cast<Fat32Node*>(dir->internal_data);
    auto* handle = new Fat32DirHandle();
    handle->entries = fat_node->fs->read_directory(fat_node->cluster, handle->count);
    handle->index = 0;
    return handle;
}

int fat32_readdir(void* h, dirent_t* out) {
    auto* handle = static_cast<Fat32DirHandle*>(h);
    if (!handle || handle->index >= handle->count) return 0;

    const auto& entry = handle->entries[handle->index];
    const char* name = entry.get_name();
    if (!name) return 0;

    strncpy(out->name, name, sizeof(out->name) - 1);
    out->name[sizeof(out->name) - 1] = '\0';

    // FAT32 attribute byte
    if (entry.is_dir()) {
        out->type = DT_DIR;
    } /*else if (attr & 0x08) {
        out->type = DT_EXEC; // optional: Volume Label / System
    }*/
    else {
        out->type = DT_FILE;
    }

    handle->index++;
    return 1;
}

void fat32_closedir(void* h) {
    const auto* handle = static_cast<Fat32DirHandle*>(h);
    if (!handle) return;

    if (handle->entries) {
        kernel::memory::free(handle->entries);
    }
    delete handle;
}

static void fat32_close(VfsNode* node) {
    if (!node) return;

    if (auto* data = static_cast<Fat32Node*>(node->internal_data)) {
        delete data;
    }

    kernel::memory::free(const_cast<char*>(node->name));
    delete node;
}

static int fat32_create(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->create_file(dir, name) ? 0 : 1;
}

static int fat32_rename(const VfsNode* node, const char* old_name, const VfsNode* new_parent, const char* new_name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    if (!dir || !old_name || !new_name) return -EINVAL;

    if (!dir->fs->rename(dir, old_name, new_name)) {
        return -EIO;  // Could not rename entry
    }

    return 0;
}

static int fat32_mkdir(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->create_directory(dir, name) ? 0 : 1;
}

static int fat32_rmdir(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->remove_directory(dir, name) ? 0 : 1;
}

static int fat32_unlink(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->delete_file(dir, name) ? 0 : 1;
}

static int fat32_stat(const VfsNode* node, vespera_stat_t* out) {
    if (!node || !out) return -EINVAL;
    const auto* fnode = static_cast<const Fat32Node*>(node->internal_data);
    if (!fnode) return -EINVAL;

    const u32 dev_id = (node->mount && node->mount->device)
                       ? node->mount->device->device_id : 0;

    return fnode->fs->stat(fnode, out, dev_id) ? 0 : -EIO;
}

static int fat32_truncate(VfsNode* node, usize new_size) {
    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EINVAL;
    u32 status = fnode->fs->truncate(fnode, new_size);
    if (status == 1) return -EIO;
    node->size = new_size;
    return 0;
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
    .stat = fat32_stat,
    .truncate = fat32_truncate,
    .poll = nullptr
};

VfsNode* wrap_fat32_root(FileSystem* fs) {
    if (!fs) return nullptr;

    const u64 usable_clusters = fs->cluster_count - 2;
    const u64 total_fs_size = usable_clusters * fs->bytes_per_cluster();

    auto* root = new Fat32Node();
    if (!root) return nullptr;
    root->fs = fs;
    root->is_dir = true;
    root->path[0] = '/';
    root->path[1] = '\0';
    root->cluster = fs->get_root_cluster();
    root->file_size = total_fs_size;

    auto* node = new VfsNode();
    if (!node) return nullptr;
    node->name = "/";
    node->mount = nullptr;
    node->type = VfsNodeType::Directory;
    node->internal_data = root;
    node->permanent = true;
    node->ops = &fat32_ops;
    node->size = total_fs_size;

    return node;
}

int fat32_probe(BlockDevice* dev, FilesystemInfo* fs_info) {
    FileSystem fs(dev);

    constexpr usize len = 11;
    memcpy(fs_info->label, fs.get_bpb()->volume_label, len);
    fs_info->label[len] = '\0';

    return fs.is_valid();
}

VfsNode* fat32_mount(BlockDevice* dev) {
    auto* fs = new FileSystem(dev);
    if (!fs->is_valid()) {
        delete fs;
        return nullptr;
    };

    VfsNode* root = wrap_fat32_root(fs);
    if (!root) {
        delete fs;
        return nullptr;
    }

    return root;
}

bool fat32_unmount(VfsNode* root) {
    if (!root) return false;

    auto* fatnode = static_cast<Fat32Node*>(root->internal_data);
    if (!fatnode) return false;

    const FileSystem* fs = fatnode->fs;
    if (!fs) return false;

    delete fs;

    delete fatnode;
    delete root;

    return true;
}

bool fat32_force_unmount(VfsNode* root) {
    if (!root) return false;

    auto* fatnode = static_cast<Fat32Node*>(root->internal_data);
    if (!fatnode) return false;

    FileSystem* fs = fatnode->fs;
    if (!fs) return false;

    fs->mark_device_lost();
    delete fs;

    delete fatnode;
    delete root;
    return true;
}

FileSystemDriver fat32_driver = {
    .name = "fat32",
    .probe = fat32_probe,
    .mount = fat32_mount,
    .unmount = fat32_unmount,
    .force_unmount = fat32_force_unmount
};
