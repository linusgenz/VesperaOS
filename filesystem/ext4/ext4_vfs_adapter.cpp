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

#include <filesystem/vfs_node.h>
#include "../vfs/fs_registry.h"
#include "ext4.h"
#include "filesystem/vfs.h"
#include "uapi/vespera/mount.h"
#include "vespera_errno.h"

using namespace ext4;

static Result<VfsNode*> ext4_find(VfsNode* node, const char* name) {
    if (!node) return Error::Inval;
    auto* dir = static_cast<Ext4Node*>(node->internal_data);
    if (!dir || !dir->is_dir) return Error::NotDir;

    usize entry_count = 0;
    FileEntry* entries = TRY(dir->fs->read_directory(dir->inode, entry_count));

    for (usize i = 0; i < entry_count; ++i) {
        if (strcmp(entries[i].get_name(), name) != 0) continue;

        auto* child_data = new Ext4Node();
        if (!child_data) {
            kernel::memory::free(entries);
            return Error::NoMem;
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
            return Error::NoMem;
        }

        child_node->name = strdup(entries[i].get_name());
        child_node->type = child_data->is_dir ? VfsNodeType::Directory : VfsNodeType::File;
        child_node->mount = node->mount;
        child_node->internal_data = child_data;
        child_node->ops = node->ops;
        child_node->size = entries[i].get_size();

        kernel::memory::free(entries);
        return Result<VfsNode*>::ok(child_node);
    }

    kernel::memory::free(entries);
    return Error::NoEnt;
}

static Result<usize> ext4_read(const VfsNode* node, usize offset, usize size, void* buf) {
    if (!node) return Error::Inval;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return Error::Inval;
    if (en->is_dir) return Error::IsDir;

    const bool update_atime = !node->mount || !(node->mount->flags & MS_NOATIME);
    return en->fs->read_file(en->inode, offset, size, buf, update_atime);
}

static Result<usize> ext4_write(VfsNode* node, usize offset, usize size, const void* buf) {
    if (!node) return Error::Inval;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return Error::Inval;
    if (en->is_dir) return Error::IsDir;

    usize written = TRY(en->fs->write_file(en->inode, offset, size, buf));
    node->size += written;
    return Result<usize>::ok(written);
}

static Result<void*> ext4_opendir(const VfsNode* node) {
    if (!node) return Error::Inval;
    const auto* dir = static_cast<const Ext4Node*>(node->internal_data);
    if (!dir) return Error::Inval;

    usize count = 0;
    FileEntry* entries = TRY(dir->fs->read_directory(dir->inode, count));

    auto* handle = new Ext4DirHandle();
    if (!handle) {
        kernel::memory::free(entries);
        return Error::NoMem;
    }

    handle->entries = entries;
    handle->count = count;
    handle->index = 0;
    return Result<void*>::ok(handle);
}

static dirent_type_t map_ext4_type(const FileEntry& fe) {
    if (fe.get_type() == DirEntryType::RegularFile && fe.is_executable()) return DT_EXEC;

    switch (fe.get_type()) {
        case DirEntryType::RegularFile:
            return DT_FILE;
        case DirEntryType::Directory:
            return DT_DIR;
        case DirEntryType::SymbolicLink:
            return DT_SYMLINK;
        case DirEntryType::CharDevice:
            return DT_CHARDEV;
        case DirEntryType::BlockDevice:
            return DT_BLOCKDEV;
        case DirEntryType::Fifo:
            return DT_FIFO;
        case DirEntryType::Socket:
            return DT_SOCKET;
        default:
            return DT_UNKNOWN;
    }
}

static Result<bool> ext4_readdir(void* dir_handle, dirent_t* out) {
    auto* h = static_cast<Ext4DirHandle*>(dir_handle);
    if (!h) return Error::Inval;

    if (h->index >= h->count) return Result<bool>::ok(false);  // end of directory

    const FileEntry& fe = h->entries[h->index++];
    const usize len = strlen(fe.get_name());
    memcpy(out->name, fe.get_name(), len);
    out->name[len] = '\0';
    out->type = map_ext4_type(fe);

    return Result<bool>::ok(true);
}

static void ext4_closedir(void* h) {
    const auto* handle = static_cast<Ext4DirHandle*>(h);
    if (!handle) return;
    if (handle->entries) kernel::memory::free(handle->entries);
    delete handle;
}

static void ext4_close(VfsNode* node) {
    if (!node) return;
    if (auto* data = static_cast<Ext4Node*>(node->internal_data)) delete data;
    kernel::memory::free(const_cast<char*>(node->name));
    delete node;
}

static VoidResult ext4_create(const VfsNode* parent, const char* name) {
    if (!parent || !name) return Error::Inval;
    const auto* dir = static_cast<const Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return Error::NotDir;

    TRY(dir->fs->create_file(dir->inode, name));
    return VoidResult::ok();
}

static VoidResult ext4_mkdir(const VfsNode* parent, const char* name) {
    if (!parent || !name) return Error::Inval;
    const auto* dir = static_cast<const Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return Error::NotDir;

    TRY(dir->fs->create_dir(dir->inode, name));
    return VoidResult::ok();
}

static VoidResult ext4_rmdir(const VfsNode* parent, const char* name) {
    if (!parent || !name) return Error::Inval;
    const auto* dir = static_cast<const Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return Error::NotDir;

    return dir->fs->rmdir(dir->inode, name);
}

static VoidResult ext4_unlink(const VfsNode* parent, const char* name) {
    if (!parent || !name) return Error::Inval;
    const auto* dir = static_cast<const Ext4Node*>(parent->internal_data);
    if (!dir || !dir->is_dir) return Error::NotDir;

    return dir->fs->unlink(dir->inode, name);
}

static VoidResult ext4_stat(const VfsNode* node, vespera_stat_t* out) {
    if (!node || !out) return Error::Inval;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return Error::Inval;

    const u32 dev_id = (node->mount && node->mount->device) ? node->mount->device->device_id : 0;
    return en->fs->stat(en->inode, out, dev_id);
}

static VoidResult ext4_truncate(VfsNode* node, usize new_size) {
    if (!node) return Error::Inval;
    auto* en = static_cast<Ext4Node*>(node->internal_data);
    if (!en || en->is_dir) return Error::Inval;

    TRY_VOID(en->fs->truncate(en->inode, new_size));

    en->file_size = new_size;
    node->size = new_size;
    return VoidResult::ok();
}

static VoidResult ext4_rename(
    const VfsNode* old_parent, const char* old_name, const VfsNode* new_parent, const char* new_name
) {
    if (!old_parent || !old_name || !new_parent || !new_name) return Error::Inval;

    const auto* old_dir = static_cast<const Ext4Node*>(old_parent->internal_data);
    const auto* new_dir = static_cast<const Ext4Node*>(new_parent->internal_data);
    if (!old_dir || !old_dir->is_dir || !new_dir || !new_dir->is_dir) return Error::NotDir;
    if (old_dir->fs != new_dir->fs) return Error::XDev;

    return old_dir->fs->rename(old_dir->inode, old_name, new_dir->inode, new_name);
}

static VoidResult ext4_chown(VfsNode* node, u32 uid, u32 gid) {
    if (!node) return Error::Inval;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return Error::Inval;

    return en->fs->chown(en->inode, uid, gid);
}

static VoidResult ext4_chmod(VfsNode* node, u16 new_mode) {
    if (!node) return Error::Inval;
    const auto* en = static_cast<const Ext4Node*>(node->internal_data);
    if (!en) return Error::Inval;

    return en->fs->chmod(en->inode, new_mode);
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
    .rename = ext4_rename,
    .mkdir = ext4_mkdir,
    .rmdir = ext4_rmdir,
    .unlink = ext4_unlink,
    .ioctl = nullptr,
    .stat = ext4_stat,
    .truncate = ext4_truncate,
    .chown = ext4_chown,
    .chmod = ext4_chmod,
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