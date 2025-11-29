// devfs.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 12.09.25.
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

#include "devfs.h"
#include <errno.h>
#include <string.h>
#include <kernel/memory.h>

#include "../../kernel/devices/chardevice.h"
#include "../vfs/vfs.h"

Vector<CharDevice *> *DevFS::devices = nullptr;
Vector<DevfsEntry *> *DevFS::nodes = nullptr;
VfsNode *DevFS::root = nullptr;
spinlock_t DevFS::lock;

static VfsNodeOps devfs_ops = {
    .read = DevFS::read,
    .write = DevFS::write,
    .find = DevFS::find,
    .close = DevFS::close,

    .opendir = DevFS::open_dir,
    .readdir = DevFS::read_dir,
    .closedir = DevFS::close_dir,

    .create = nullptr,
    .rename = nullptr,
    .mkdir = nullptr,
    .rmdir = nullptr,
    .unlink = nullptr,

    .ioctl = DevFS::ioctl,
};

void DevFS::init() {
    lock.init();
    lock_debug_register(&lock, "devfs_lock");
    devices = new Vector<CharDevice *>(8);
    nodes = new Vector<DevfsEntry *>(16);

    root = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    root->name = "dev";
    root->type = VfsNodeType::Directory;
    root->internal_data = nullptr;
    root->permanent = true;
    root->ops = &devfs_ops;

    VFS::mount_virtual(root, "/dev");
}

VfsNode *DevFS::ensure_bus_dir(BusType bus) {
    const char *bus_name = bus_to_str(bus);

    if (bus == BUS_NONE || bus == VIRTUAL) return root;

    for (auto *e: *nodes) {
        if (e->is_bus_dir && strcmp(e->node->name, bus_name) == 0) {
            return e->node;
        }
    }

    auto *dir = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    dir->name = strdup(bus_name);
    dir->type = VfsNodeType::Directory;
    dir->ops = &devfs_ops;
    dir->internal_data = nullptr;

    auto *entry = (DevfsEntry *) kernel::memory::malloc(sizeof(DevfsEntry));
    entry->dev = nullptr;
    entry->node = dir;
    entry->cf = nullptr;
    entry->is_bus_dir = true;
    entry->bus_type = bus;
    entry->parent = root;

    nodes->push_back(entry);
    return dir;
}


CharDevice *DevFS::lookup(const char *name) {
    for (const auto dev : *devices) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
    }
    return nullptr;
}

int DevFS::register_device(CharDevice *dev) {
    if (!dev || strlen(dev->name) == 0) return -EINVAL;
    spinlock_guard guard(lock);

    if (lookup(dev->name)) return -EEXIST;

    devices->push_back(dev);

    VfsNode *bus_dir = ensure_bus_dir(dev->bus_type);

    VfsNode *node = create_node(dev->name, bus_dir);
    if (!node) return -ENOMEM;

    return SUCCESS_CODE;
}

int DevFS::unregister_device(const char *name) {
    spinlock_guard guard(lock);

    for (size_t i = 0; i < devices->size(); i++) {
        if (const CharDevice *dev = (*devices)[i]; strcmp(dev->name, name) == 0) {
            devices->erase(i);
            remove_node(dev->name);
            return SUCCESS_CODE;
        }
    }

    return -ENOENT;
}


VfsNode *DevFS::create_node(const char *dev_name, VfsNode* parent) {
    CharDevice *d = lookup(dev_name);
    if (!d) return nullptr;

    auto *n = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    n->name = dev_name;
    n->type = VfsNodeType::Device;

    auto *e = static_cast<DevfsEntry*>(kernel::memory::malloc(sizeof(DevfsEntry)));
    e->dev = d;
    e->node = n;
    e->cf = nullptr;
    e->is_bus_dir = false;
    e->bus_type = d->bus_type;

    n->internal_data = e;
    n->ops = &devfs_ops;
    n->permanent = false;

    e->parent = parent;

    // register in /dev
    nodes->push_back(e);
    return n;
}

int DevFS::remove_node(const char *dev_name) {
    spinlock_guard guard(lock);

    for (size_t i = 0; i < nodes->size(); i++) {
        DevfsEntry *e = (*nodes)[i];
        if (strcmp(e->node->name, dev_name) == 0) {
            nodes->erase(i);

            kernel::memory::free(e->node);
            kernel::memory::free(e);

            return SUCCESS_CODE;
        }
    }

    return -ENOENT;
}

const char *DevFS::alloc_unique_name(const char *base) {
    spinlock_guard guard(lock);

    static char buffer[64];
    strncpy(buffer, base, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    snprintf(buffer, sizeof(buffer), "%s0", base);

    int counter = 0;
    while (lookup(buffer) != nullptr) {
        snprintf(buffer, sizeof(buffer), "%s%d", base, counter++);
    }

    const size_t len = strlen(buffer) + 1;
    const auto result = static_cast<char*>(kernel::memory::malloc(len));
    strncpy(result, buffer, len);
    return result;
}

int DevFS::open(const VfsNode *node) {
    if (!node) return -1;

    auto *entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->dev) return -1;

    if (!entry->cf) {
        entry->cf = new CharFile();
        entry->cf->driver_private = nullptr;
        return entry->dev->open(&entry->cf);
    }

    return 0;
}

ssize_t DevFS::read(const VfsNode *node, const size_t offset, const size_t size, void *buffer) {
    if (!node) return 0;

    const auto *entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->dev) return 0;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        open(node);
    }
    return entry->dev->read(entry->cf, buffer, size, offset);
}


ssize_t DevFS::write(VfsNode *node, size_t offset, const size_t size, const void *buffer) {
    if (!node) return 0;

    const auto *entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->dev) return 0;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        open(node);
    }

    return entry->dev->write(entry->cf, buffer, size);
}

ssize_t DevFS::ioctl(VfsNode *node, uint32_t cmd, void *arg) {
    if (!node) return 0;
    const auto *entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->dev) return 0;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        open(node);
    }

    return entry->dev->ioctl(entry->cf, cmd, arg);
}

VfsNode *DevFS::find(VfsNode *dir, const char *name) {
    if (!dir || !name) return nullptr;
    for (const auto &node: *nodes) {
        if (node->parent != dir) continue;
        if (strcmp(node->node->name, name) == 0) {
            return node->node;
        }
    }
    return nullptr;
}

void DevFS::close(VfsNode *node) {
    if (!node) return;

    auto *entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->dev || !entry->cf) return;

    spinlock_guard guard(lock);
    entry->dev->release(entry->cf);
    delete entry->cf;
    entry->cf = nullptr;
}

struct DevfsDirHandle {
    size_t index;
    VfsNode *dir_node;
};

void *DevFS::open_dir(VfsNode *dir) {
    auto *h = (DevfsDirHandle *) kernel::memory::malloc(sizeof(DevfsDirHandle));
    h->index = 0;
    h->dir_node = dir;
    return h;
}

int DevFS::read_dir(void *dir_handle, dirent_t *out) {
    auto* h = (DevfsDirHandle*) dir_handle;
    VfsNode* dir = h->dir_node;

    size_t count = 0;
    for (const auto* e : *nodes) {
        if (e->parent != dir) continue;

        if (count == h->index) {
            strncpy(out->name, e->node->name, sizeof(out->name) - 1);
            out->name[sizeof(out->name) - 1] = '\0';

            if (e->node->type == VfsNodeType::Directory) {
                out->type = DT_DIR;
            } else if (e->node->type == VfsNodeType::Device) {
                out->type = DT_CHARDEV;
            } else if (e->node->type == VfsNodeType::Device) {
                out->type = DT_BLOCKDEV;
            } else {
                out->type = DT_FILE;
            }

            h->index++;
            return 1;
        }
        count++;
    }

    return 0;
}



void DevFS::close_dir(void *dir_handle) {
    kernel::memory::free(dir_handle);
}
