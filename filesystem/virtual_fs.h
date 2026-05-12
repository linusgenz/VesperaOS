/**
 * @file virtual_fs.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VESPERAOS_VIRTUAL_FS_H
#define VESPERAOS_VIRTUAL_FS_H

#include <klib/string.h>
#include <klib/vector.h>
#include <uapi/vespera/dirent.h>
#include <vespera/sync/spinlock.h>

#include <filesystem/vfs.h>

struct DirData {
    Vector<VfsNode*> subdirs;
    Vector<VfsNode*> files;
};

// Base entry structure
template <typename DeviceType>
struct VirtualFsEntry {
    DeviceType* device;
    VfsNode* node;
    void* handle;
    bool is_directory;
    VfsNode* parent;
};

// Template base class for virtual filesystems
template <typename DeviceType, typename EntryType = VirtualFsEntry<DeviceType>>
class VirtualFilesystem {
   protected:
    static VfsNode* root_;
    static Spinlock lock_;
    static VfsNodeOps ops_;

    // Lookup a device by name
    static VfsNode* lookup_device(const VfsNode* dir, const char* name) {
        if (!dir || !name) return nullptr;

        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return nullptr;

        for (auto* file_node : data->files) {
            auto* entry = static_cast<EntryType*>(file_node->internal_data);
            if (entry && entry->device && strcmp(entry->device->name, name) == 0) return file_node;
        }

        for (auto* subdir : data->subdirs) {
            if (auto* found = lookup_device(subdir, name)) return found;
        }

        return nullptr;
    }

    // Create a subdirectory within the virtual filesystem
    static VfsNode* ensure_subdirectory(const char* name, VfsNode* parent = nullptr) {
        if (!parent) parent = root_;

        auto* parent_data = static_cast<DirData*>(parent->internal_data);
        if (!parent_data) {
            parent_data = new DirData();
            parent->internal_data = parent_data;
        }

        for (auto* sub : parent_data->subdirs) {
            if (strcmp(sub->name, name) == 0) return sub;
        }

        auto* dir = new VfsNode();
        dir->name = name;
        dir->mount = nullptr;
        dir->type = VfsNodeType::Directory;
        dir->ops = &ops_;
        dir->permanent = true;

        auto* data = new DirData();
        dir->internal_data = data;

        parent_data->subdirs.push_back(dir);

        return dir;
    }

    // Create a node for a device entry
    static EntryType* create_entry_node(const char* name, VfsNode* parent, DeviceType* dev, VfsNodeType type) {
        auto* n = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
        n->name = name;
        n->type = type;
        n->ops = &ops_;
        n->permanent = false;

        auto* e = static_cast<EntryType*>(kernel::memory::malloc(sizeof(EntryType)));
        memset(e, 0, sizeof(EntryType));
        e->device = dev;
        e->node = n;
        e->parent = parent;
        e->is_directory = false;

        n->internal_data = e;

        auto* parent_data = static_cast<DirData*>(parent->internal_data);
        if (!parent_data) {
            parent_data = new DirData();
            parent->internal_data = parent_data;
        }

        parent_data->files.push_back(n);
        return e;
    }

    static void delete_entry_node(VfsNode* node) {
        if (!node) return;

        if (node->type == VfsNodeType::Directory) {
            if (auto* dir_data = static_cast<DirData*>(node->internal_data)) {
                for (auto* sub : dir_data->subdirs) delete_entry_node(sub);
                dir_data->subdirs.clear();

                for (auto* file : dir_data->files) {
                    auto* entry = static_cast<EntryType*>(file->internal_data);
                    if (entry) {
                        delete entry->device;
                        delete entry;
                    }
                    delete file;
                }
                dir_data->files.clear();

                delete dir_data;
            }
        } else {
            auto* entry = static_cast<EntryType*>(node->internal_data);
            if (entry) {
                delete entry->device;
                delete entry;
            }
        }

        delete node;
    }

   public:
    static void init(const char* mount_point, const char* name) {
        lock_.init("vfs_lock");

        root_ = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
        root_->name = strdup(name);
        root_->type = VfsNodeType::Directory;
        root_->mount = nullptr;
        root_->permanent = true;
        root_->ops = &ops_;

        auto* data = new DirData();
        root_->internal_data = data;

        VFS::mount_virtual(root_, mount_point);
    }

    static Result<VfsNode*> find(VfsNode* dir, const char* name) {
        if (!dir || !name) return Error::Inval;
        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return Error::Inval;

        for (auto* sub : data->subdirs)
            if (strcmp(sub->name, name) == 0) return Result<VfsNode*>::ok(sub);

        for (auto* file_node : data->files)
            if (strcmp(file_node->name, name) == 0) return Result<VfsNode*>::ok(file_node);

        return Error::NoEnt;
    }

    static Result<VfsNode*> finddir(const VfsNode* dir, const char* name) {
        if (!dir || !name) return Result<VfsNode*>::err(Error::Inval);

        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return Result<VfsNode*>::err(Error::Inval);

        for (auto* sub : data->subdirs) {
            if (strcmp(sub->name, name) == 0) return Result<VfsNode*>::ok(sub);
        }

        return Result<VfsNode*>::err(Error::NoEnt);
    }

    // Directory operations
    struct DirHandle {
        usize index;
        const VfsNode* dir_node;
    };

    static Result<void*> open_dir(const VfsNode* dir) {
        auto* h = static_cast<DirHandle*>(kernel::memory::malloc(sizeof(DirHandle)));
        if (!h) return Error::NoMem;
        h->index = 0;
        h->dir_node = dir;
        return Result<void*>::ok(h);
    }

    static Result<bool> read_dir(void* dir_handle, dirent_t* out) {
        auto* h = static_cast<DirHandle*>(dir_handle);
        if (!h) return Error::Inval;

        auto* dir = h->dir_node;
        if (!dir || !dir->internal_data) return Error::Inval;

        auto* data = static_cast<DirData*>(dir->internal_data);
        usize idx = h->index;

        if (idx < data->subdirs.size()) {
            VfsNode* sub = data->subdirs[idx];
            strncpy(out->name, sub->name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            out->type = DT_DIR;
            ++h->index;
            return Result<bool>::ok(true);
        }

        idx -= data->subdirs.size();

        if (idx < data->files.size()) {
            VfsNode* file = data->files[idx];
            strncpy(out->name, file->name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            out->type = VFS::node_type_to_dirent_type(file->type);
            ++h->index;
            return Result<bool>::ok(true);
        }

        return Result<bool>::ok(false);
    }

    static void close_dir(void* dir_handle) {
        kernel::memory::free(dir_handle);
    }

    static VoidResult stat(const VfsNode*, vespera_stat_t* out) {
        out->dev_id = 0;
        out->inode_id = 0;
        out->block_size = 0;
        out->blocks = 0;
        out->size = 0;
        out->mode = 0x41ED;  // 0100755 octal — dir + rwxr-xr-x
        return VoidResult::ok();
    }
};

template <typename D, typename E>
VfsNode* VirtualFilesystem<D, E>::root_ = nullptr;

template <typename D, typename E>
Spinlock VirtualFilesystem<D, E>::lock_;

template <typename D, typename E>
VfsNodeOps VirtualFilesystem<D, E>::ops_ = {};

#endif  // VESPERAOS_VIRTUAL_FS_H
