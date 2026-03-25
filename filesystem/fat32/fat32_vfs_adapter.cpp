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

    // Offset can be >= actual → EOF
    if (offset >= actual) return 0;

    usize copy_size = actual - offset;
    if (copy_size > size) copy_size = size;

    return static_cast<isize>(copy_size);
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
            auto* child_data = static_cast<Fat32Node*>(kernel::memory::malloc(sizeof(Fat32Node)));
            memset(child_data, 0, sizeof(Fat32Node));
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

            auto* child = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
            child->name = entries[i].get_name();
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
        kernel::memory::free(data);
    }

    kernel::memory::free(node);
}

static int fat32_create(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->create_file(dir, name) ? 0 : -1;
}

static int fat32_rename(const VfsNode* node, const char* old_name, const char* new_name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    if (!dir || !old_name || !new_name) return -EINVAL;

    if (!dir->fs->rename(dir, old_name, new_name)) {
        return -EIO;  // Could not rename entry
    }

    return 0;
}

static int fat32_mkdir(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->create_directory(dir, name) ? 0 : -1;
}

static int fat32_rmdir(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->remove_directory(dir, name) ? 0 : -1;
}

static int fat32_unlink(const VfsNode* node, const char* name) {
    const auto* dir = static_cast<Fat32Node*>(node->internal_data);
    return dir->fs->delete_file(dir, name) ? 0 : -1;
}

static int fat32_stat(const VfsNode* node, vespera_stat_t* out) {
    const auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EINVAL;

    out->inode_id = fnode->cluster;
    out->block_size = fnode->fs->bytes_per_cluster();

    // Number of allocated clusters * sectors per cluster * 512
    if (fnode->file_size > 0 && out->block_size > 0) {
        const u64 clusters_used = (fnode->file_size + out->block_size - 1) / out->block_size;
        const u32 sectors_per_cluster = fnode->fs->get_bpb()->sectors_per_cluster;
        out->blocks = clusters_used * sectors_per_cluster;
    }

    out->dev_id = node->mount->device->device_id;

    return 0;
}

static int fat32_truncate(VfsNode* node, usize new_size) {
    auto* fnode = static_cast<Fat32Node*>(node->internal_data);
    if (!fnode) return -EINVAL;

    const u32 cluster_bytes = fnode->fs->bytes_per_cluster();

    if (new_size == 0) {
        if (fnode->cluster != 0) {
            fnode->fs->trim_cluster_chain(fnode->cluster);
            fnode->fs->free_cluster_chain(fnode->cluster);
            fnode->cluster = 0;
        }
    } else {
        const usize needed = (new_size + cluster_bytes - 1) / cluster_bytes;
        usize count = 0;
        if (u32* chain = fnode->fs->get_cluster_chain(fnode->cluster, count); chain && count > needed) {
            fnode->fs->write_fat_entry(chain[needed - 1], 0x0FFFFFFF);
            for (usize i = needed; i < count; ++i) fnode->fs->write_fat_entry(chain[i], 0);
            kernel::memory::free(chain);
        }
    }

    fnode->file_size = new_size;
    fnode->dir_entry.file_size = static_cast<u32>(new_size);
    fnode->dir_entry.first_cluster_low = static_cast<u16>(fnode->cluster & 0xFFFF);
    fnode->dir_entry.first_cluster_high = static_cast<u16>((fnode->cluster >> 16) & 0xFFFF);
    update_write_time(fnode->dir_entry);

    node->size = new_size;
    return fnode->fs->overwrite_directory_entry(fnode->parent_cluster, fnode->current_index, &fnode->dir_entry) ? 0
                                                                                                                : -EIO;
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

    auto* root = static_cast<Fat32Node*>(kernel::memory::malloc(sizeof(Fat32Node)));
    root->fs = fs;
    root->is_dir = true;
    root->path[0] = '/';
    root->path[1] = '\0';
    root->cluster = fs->get_root_cluster();
    root->file_size = total_fs_size;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
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

    return wrap_fat32_root(fs);
}

bool fat32_unmount(VfsNode* root) {
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

bool fat32_force_unmount(VfsNode* root) {
    if (!root) return false;

    auto* fatnode = static_cast<Fat32Node*>(root->internal_data);
    if (!fatnode) return false;

    FileSystem* fs = fatnode->fs;
    if (!fs) return false;

    fs->mark_device_lost();
    delete fs;

    kernel::memory::free(fatnode);
    kernel::memory::free(root);
    return true;
}

FileSystemDriver fat32_driver = {
    .name = "fat32",
    .probe = fat32_probe,
    .mount = fat32_mount,
    .unmount = fat32_unmount,
    .force_unmount = fat32_force_unmount
};
