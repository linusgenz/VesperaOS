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

#include <kernel/sync/spinlock.h>
#include <vector.h>

#include "errno.h"
#include "vfs/vfs.h"

struct DirData
{
    Vector<VfsNode*> subdirs;
    Vector<VfsNode*> files;
};

// Base entry structure
template <typename DeviceType>
struct VirtualFsEntry
{
    DeviceType* device;
    VfsNode* node;
    void* handle;
    bool is_directory;
    VfsNode* parent;
};

// Template base class for virtual filesystems
template <typename DeviceType, typename EntryType = VirtualFsEntry<DeviceType>>
class VirtualFilesystem
{
protected:
    static VfsNode* root;
    static spinlock_t lock;
    static VfsNodeOps ops;

    // Lookup a device by name
    static VfsNode* lookup_device(const VfsNode* dir, const char* name)
    {
        if (!dir || !name) return nullptr;

        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return nullptr;

        for (auto* file_node : data->files)
        {
            auto* entry = static_cast<EntryType*>(file_node->internal_data);
            if (entry && entry->device && strcmp(entry->device->name, name) == 0)
                return file_node;
        }

        for (auto* subdir : data->subdirs)
        {
            if (auto* found = lookup_device(subdir, name))
                return found;
        }

        return nullptr;
    }

    // Create a subdirectory within the virtual filesystem
    static VfsNode* ensure_subdirectory(const char* name, VfsNode* parent = nullptr)
    {
        if (!parent) parent = root;

        auto* parent_data = static_cast<DirData*>(parent->internal_data);
        if (!parent_data)
        {
            parent_data = new DirData();
            parent->internal_data = parent_data;
        }

        for (auto* sub : parent_data->subdirs)
        {
            if (strcmp(sub->name, name) == 0)
                return sub;
        }

        auto* dir = new VfsNode();
        dir->name = name;
        dir->type = VfsNodeType::Directory;
        dir->ops = &ops;
        dir->permanent = true;

        auto* data = new DirData();
        dir->internal_data = data;

        parent_data->subdirs.push_back(dir);

        return dir;
    }


    // Create a node for a device entry
    static EntryType* create_entry_node(const char* name, VfsNode* parent, DeviceType* dev, VfsNodeType type)
    {
        auto* n = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
        n->name = name;
        n->type = type;
        n->ops = &ops;
        n->permanent = false;

        auto* e = static_cast<EntryType*>(kernel::memory::malloc(sizeof(EntryType)));
        memset(e, 0, sizeof(EntryType));
        e->device = dev;
        e->node = n;
        e->parent = parent;
        e->is_directory = false;

        n->internal_data = e;

        auto* parent_data = static_cast<DirData*>(parent->internal_data);
        if (!parent_data)
        {
            parent_data = new DirData();
            parent->internal_data = parent_data;
        }

        parent_data->files.push_back(n);
        return e;
    }

    static void delete_entry_node(VfsNode* node)
    {
        if (!node) return;

        if (node->type == VfsNodeType::Directory)
        {
            auto* dir_data = static_cast<DirData*>(node->internal_data);
            if (dir_data)
            {
                for (auto* sub : dir_data->subdirs)
                    delete_entry_node(sub);
                dir_data->subdirs.clear();

                for (auto* file : dir_data->files)
                {
                    auto* entry = static_cast<EntryType*>(file->internal_data);
                    if (entry)
                    {
                        delete entry->device;
                        delete entry;
                    }
                    delete file;
                }
                dir_data->files.clear();

                delete dir_data;
            }
        }
        else
        {
            auto* entry = static_cast<EntryType*>(node->internal_data);
            if (entry)
            {
                delete entry->device;
                delete entry;
            }
        }

        delete node;
    }

public:
    static void init(const char* mount_point, const char* name)
    {
        lock.init("vfs_lock");

        root = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
        root->name = strdup(name);
        root->type = VfsNodeType::Directory;
        root->permanent = true;
        root->ops = &ops;

        auto* data = new DirData();
        root->internal_data = data;

        VFS::mount_virtual(root, mount_point);
    }

    static VfsNode* find(const VfsNode* dir, const char* name)
    {
        if (!dir || !name) return nullptr;
        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return nullptr;

        for (auto* sub : data->subdirs)
        {
            if (strcmp(sub->name, name) == 0) return sub;
        }
        for (auto* file_node : data->files)
        {
            if (strcmp(file_node->name, name) == 0) return file_node;
        }
        return nullptr;
    }

    static VfsNode* finddir(const VfsNode* dir, const char* name)
    {
        if (!dir || !name) return nullptr;
        auto* data = static_cast<DirData*>(dir->internal_data);
        if (!data) return nullptr;

        for (auto* sub : data->subdirs)
        {
            if (strcmp(sub->name, name) == 0) return sub;
        }
        return nullptr;
    }

    // Directory operations
    struct DirHandle
    {
        size_t index;
        const VfsNode* dir_node;
    };

    static void* open_dir(const VfsNode* dir)
    {
        auto* h = static_cast<DirHandle*>(kernel::memory::malloc(sizeof(DirHandle)));
        h->index = 0;
        h->dir_node = dir;
        return h;
    }

    static int read_dir(void* dir_handle, dirent_t* out)
    {
        auto* h = static_cast<DirHandle*>(dir_handle);
        auto* dir = h->dir_node;
        if (!dir || !dir->internal_data) return 0;

        auto* data = static_cast<DirData*>(dir->internal_data);

        size_t idx = h->index;

        if (idx < data->subdirs.size())
        {
            VfsNode* sub = data->subdirs[idx];

            strncpy(out->name, sub->name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            out->type = DT_DIR;

            ++h->index;
            return 1;
        }

        idx -= data->subdirs.size();

        if (idx < data->files.size())
        {
            VfsNode* file = data->files[idx];

            strncpy(out->name, file->name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';
            out->type = VFS::node_type_to_dirent_type(file->type);

            ++h->index;
            return 1;
        }

        return 0;
    }

    static void close_dir(void* dir_handle)
    {
        kernel::memory::free(dir_handle);
    }
};

template <typename D, typename E>
VfsNode* VirtualFilesystem<D, E>::root = nullptr;

template <typename D, typename E>
spinlock_t VirtualFilesystem<D, E>::lock;

template <typename D, typename E>
VfsNodeOps VirtualFilesystem<D, E>::ops = {};

#endif //VESPERAOS_VIRTUAL_FS_H
