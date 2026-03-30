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
#include "uapi/vespera/mount.h"
#include "vespera/filesystem/vfs.h"
#include "vespera_errno.h"

using namespace ext4;

static VfsNode* ext4_find(VfsNode* node, const char* name) {
    if (!node) {
        Log::debug("no node");
        return nullptr;
    };

    auto* dir = static_cast<Ext4Node*>(node->internal_data);
    if (!dir || !dir->is_dir) {
        Log::debug("not dir: %p, %u", dir, dir ? dir->is_dir : 0);
        return nullptr;
    };

    usize entry_count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, entry_count);
    // Log::debug("entries=%p", entries);
    if (!entries) return nullptr;

    for (usize i = 0; i < entry_count; ++i) {
        if (strcmp(entries[i].get_name(), name) != 0) continue;

        auto* child_data = new Ext4Node();
        if (!child_data) {
            kernel::memory::free(entries);
            return nullptr;
        }

        child_data->fs = dir->fs;
        child_data->inode = entries[i].get_inode();
        child_data->is_dir = entries[i].is_dir();
        child_data->file_size = entries[i].get_size();
        child_data->entries = nullptr;
        child_data->entry_count = 0;
        child_data->current_index = 0;

        const bool root = (dir->path[0] == '/' && dir->path[1] == '\0');
        snprintf(child_data->path, sizeof(child_data->path), "%s%s%s", dir->path, root ? "" : "/", name);

        auto* child_node = new VfsNode();
        if (!child_node) {
            delete child_data;
            kernel::memory::free(entries);
            return nullptr;
        }

        child_node->name = strdup(entries[i].get_name());
        child_node->type = child_data->is_dir ? VfsNodeType::Directory : VfsNodeType::File;
        child_node->mount = node->mount;
        child_node->internal_data = child_data;
        child_node->ops = node->ops;
        child_node->size = entries[i].get_size();

        kernel::memory::free(entries);
        return child_node;
    }

    kernel::memory::free(entries);
    return nullptr;
}

static isize ext4_read(const VfsNode* node, usize offset, usize size, void* buf) {
    if (!node) return -1;
    const auto* en = static_cast<Ext4Node*>(node->internal_data);
    if (!en || en->is_dir) return -1;

    const bool update_atime = !node->mount || !(node->mount->flags & MS_NOATIME);

    return en->fs->read_file(en->inode, offset, size, buf, update_atime);
}

static isize ext4_write(VfsNode* node, usize offset, usize size, const void* buf) {
    if (!node) return -1;
    const auto* en = static_cast<Ext4Node*>(node->internal_data);
    if (!en || en->is_dir) return -1;

    return en->fs->write_file(en->inode, offset, size, buf);
}

static void* ext4_opendir(const VfsNode* node) {
    if (!node) return nullptr;
    const auto* dir = static_cast<const Ext4Node*>(node->internal_data);
    if (!dir) return nullptr;

    usize count = 0;
    FileEntry* entries = dir->fs->read_directory(dir->inode, count);
    if (!entries) return nullptr;

    auto* handle = new Ext4DirHandle();
    if (!handle) {
        kernel::memory::free(entries);
        return nullptr;
    }

    handle->entries = entries;
    handle->count = count;
    handle->index = 0;
    return handle;
}

static dirent_type_t map_ext4_type(const FileEntry& fe) {
    if (fe.get_type() == DirEntryType::RegularFile && fe.is_executable())
        return DT_EXEC;

    switch (fe.get_type()) {
        case DirEntryType::RegularFile:  return DT_FILE;
        case DirEntryType::Directory:    return DT_DIR;
        case DirEntryType::SymbolicLink: return DT_SYMLINK;
        case DirEntryType::CharDevice:   return DT_CHARDEV;
        case DirEntryType::BlockDevice:  return DT_BLOCKDEV;
        case DirEntryType::Fifo:         return DT_FIFO;
        case DirEntryType::Socket:       return DT_SOCKET;
        default:                         return DT_UNKNOWN;
    }
}

static int ext4_readdir(void* dir_handle, dirent_t* out) {
    auto* h = static_cast<Ext4DirHandle*>(dir_handle);
    if (!h || h->index >= h->count) return 0;

    const FileEntry& fe = h->entries[h->index++];
    const usize len = strlen(fe.get_name());
    memcpy(out->name, fe.get_name(), len);
    out->name[len] = '\0';
    out->type = map_ext4_type(fe);

    return 1;
}

static void ext4_closedir(void* h) {
    const auto* handle = static_cast<Ext4DirHandle*>(h);
    if (!handle) return;

    if (handle->entries) {
        kernel::memory::free(handle->entries);
    }
    delete handle;
}

static void ext4_close(VfsNode* node) {
    if (!node) return;

    if (auto* data = static_cast<Ext4Node*>(node->internal_data)) {
        delete data;
    }

    kernel::memory::free(const_cast<char*>(node->name));
    delete node;
}

static int ext4_create(const VfsNode* parent, const char* name) {
    if (!parent || !name) return 1;
    const auto* dir = static_cast<Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return 1;

    if (const u32 new_inode = dir->fs->create_file(dir->inode, name); new_inode == 0) return 1;

    return 0;
}

static int ext4_mkdir(const VfsNode* parent, const char* name) {
    if (!parent || !name) return 1;
    const auto* dir = static_cast<Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return 1;

    if (const u32 new_inode = dir->fs->create_dir(dir->inode, name); new_inode == 0) return 1;

    return 0;
}

static int ext4_rmdir(const VfsNode* parent, const char* name) {
    if (!parent || !name) return 1;
    const auto* dir = static_cast<Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return 1;
    return dir->fs->rmdir(dir->inode, name) ? 0 : 1;
}

static int ext4_unlink(const VfsNode* parent, const char* name) {
    if (!parent || !name) return 1;
    const auto* dir = static_cast<Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return 1;
    return dir->fs->unlink(dir->inode, name) ? 0 : 1;
}

static int ext4_stat(const VfsNode* node, vespera_stat_t* out) {
    if (!node || !out) return -EINVAL;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return -EINVAL;

    const u32 dev_id = (node->mount && node->mount->device) ? node->mount->device->device_id : 0;

    return en->fs->stat(en->inode, out, dev_id) ? 0 : -EIO;
}

static int ext4_truncate(VfsNode* node, const usize new_size) {
    if (!node) return -EINVAL;
    auto* en = static_cast<Ext4Node*>(node->internal_data);
    if (!en || en->is_dir) return -EINVAL;

    if (!en->fs->truncate(en->inode, new_size)) return -EIO;

    en->file_size = new_size;
    node->size = new_size;
    return 0;
}

static VfsNodeOps ext4_ops = {
    .read = ext4_read,
    .write = ext4_write,
    .find = ext4_find,
    .close = ext4_close,
    .opendir = ext4_opendir,
    .readdir = ext4_readdir,
    .closedir = ext4_closedir,
    .create = ext4_create,
    .rename = nullptr,
    .mkdir = ext4_mkdir,
    .rmdir = ext4_rmdir,
    .unlink = ext4_unlink,
    .ioctl = nullptr,
    .stat = ext4_stat,
    .truncate = ext4_truncate,
    .poll = nullptr,
};

static VfsNode* wrap_ext4_root(FileSystem* fs) {
    if (!fs) return nullptr;

    const u32 block_size = fs->get_block_size();
    const u64 total_fs_size = static_cast<u64>(fs->get_superblock()->s_blocks_count_lo) * block_size;

    auto* root_data = new Ext4Node();
    if (!root_data) return nullptr;

    root_data->fs = fs;
    root_data->inode = EXT4_ROOT_INODE;
    root_data->is_dir = true;
    root_data->file_size = total_fs_size;
    root_data->entries = nullptr;
    root_data->entry_count = 0;
    root_data->current_index = 0;
    snprintf(root_data->path, sizeof(root_data->path), "/");

    auto* root_node = new VfsNode();
    if (!root_node) {
        delete root_data;
        return nullptr;
    }

    root_node->name = "/";
    root_node->type = VfsNodeType::Directory;
    root_node->mount = nullptr;
    root_node->internal_data = root_data;
    root_node->size = total_fs_size;
    root_node->permanent = true;
    root_node->ops = &ext4_ops;

    return root_node;
}

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

    VfsNode* root = wrap_ext4_root(fs);
    if (!root) {
        delete fs;
        return nullptr;
    }

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