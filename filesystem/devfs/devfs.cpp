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

#include "../../kernel/devices/chardevice.h"
#include "../vfs/vfs.h"

Vector<CharDevice*>* DevFS::devices = nullptr;
Vector<DevfsEntry*>* DevFS::nodes = nullptr;
VfsNode* DevFS::root = nullptr;
spinlock_t DevFS::lock;

static VfsNodeOps devfs_ops = {
    .read = DevFS::read,
    .write = DevFS::write,
    .find = DevFS::find,
    .close = DevFS::close,

    .file_size = nullptr,
    .opendir = DevFS::open_dir,
    .readdir = DevFS::read_dir,
    .closedir = DevFS::close_dir,

    .create = nullptr,
    .rename = nullptr,
    .mkdir = nullptr,
    .rmdir = nullptr,
    .unlink = nullptr
};

void DevFS::init() {
    lock.init();
    devices = new Vector<CharDevice*>(8);
    nodes   = new Vector<DevfsEntry*>(16);

    root = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    root->name = "dev";
    root->type = VfsNodeType::Directory;
    root->internal_data = nullptr;
    root->permanent = true;
    root->ops = &devfs_ops;

    vfs_mount_virtual(root, "/dev");
}

CharDevice* DevFS::lookup(const char* name) {
    for (size_t i = 0; i < devices->size(); i++) {
        CharDevice* dev = (*devices)[i];
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

    VfsNode* node = create_node(dev->name);
    if (!node) return -ENOMEM;

    return SUCCESS_CODE;
}

int DevFS::unregister_device(const char *name) {
    spinlock_guard guard(lock);

    for (size_t i = 0; i < devices->size(); i++) {
        CharDevice* dev = (*devices)[i];
        if (strcmp(dev->name, name) == 0) {
            devices->erase(i);
            remove_node(dev->name);
            return SUCCESS_CODE;
        }
    }

    return -ENOENT;
}


VfsNode *DevFS::create_node(const char *dev_name) {
    CharDevice *d = lookup(dev_name);
    if (!d) return nullptr;

    VfsNode *n = (VfsNode *) kernel::memory::malloc(sizeof(VfsNode));
    n->name = strdup(dev_name);
    n->type = VfsNodeType::Device;

    DevfsEntry *e = (DevfsEntry *) kernel::memory::malloc(sizeof(DevfsEntry));
    e->dev = d;
    e->node = n;
    e->cf = nullptr;

    n->internal_data = e;
    n->ops = &devfs_ops;
    n->permanent = false;

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

            if (e->node->name) {
                kernel::memory::free((void *) e->node->name);
            }
            kernel::memory::free(e->node);
            kernel::memory::free(e);

            return SUCCESS_CODE;
        }
    }

    return -ENOENT;
}

const char* DevFS::alloc_unique_name(const char* base) {
    spinlock_guard guard(lock);

    static char buffer[64];
    strncpy(buffer, base, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';

    snprintf(buffer, sizeof(buffer), "%s0", base);

    int counter = 0;
    while (lookup(buffer) != nullptr) {
        snprintf(buffer, sizeof(buffer), "%s%d", base, counter++);
    }

    size_t len = strlen(buffer)+1;
    char* result = (char*) kernel::memory::malloc(len);
    strncpy(result, buffer, len);
    return result;
}

int DevFS::open(VfsNode* node) {
    if (!node) return -1;

    DevfsEntry* entry = (DevfsEntry*) node->internal_data;
    if (!entry || !entry->dev) return -1;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        entry->cf = new CharFile();
        entry->cf->driver_private = nullptr;
        return entry->dev->open(&entry->cf);
    }

    return 0;
}

size_t DevFS::read(VfsNode* node, size_t offset, size_t size, void* buffer) {
    if (!node) return 0;

    DevfsEntry* entry = (DevfsEntry*) node->internal_data;
    if (!entry || !entry->dev) return 0;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        open(node);
    }

    return entry->dev->read(entry->cf, buffer, size, offset);
}



size_t DevFS::write(VfsNode* node, size_t offset, size_t size, const void* buffer) {
    if (!node) return 0;

    DevfsEntry* entry = (DevfsEntry*) node->internal_data;
    if (!entry || !entry->dev) return 0;

    spinlock_guard guard(lock);

    if (!entry->cf) {
        open(node);
    }

    return entry->dev->write(entry->cf, buffer, size);
}




VfsNode *DevFS::find(VfsNode *dir, const char *name) {
    if (dir != root) return nullptr;
    for (const auto & node : *nodes) {
        if (strcmp(node->node->name, name) == 0) {
            return node->node;
        }
    }
    return nullptr;
}

void DevFS::close(VfsNode* node) {
    if (!node) return;

    DevfsEntry* entry = (DevfsEntry*) node->internal_data;
    if (!entry || !entry->dev || !entry->cf) return;

    spinlock_guard guard(lock);
    entry->dev->release(entry->cf);
    delete entry->cf;
    entry->cf = nullptr;
}



struct DevfsDirHandle {
    size_t index;
};

void *DevFS::open_dir(VfsNode *dir) {
    if (dir != root) return nullptr;
    DevfsDirHandle *h = (DevfsDirHandle *) kernel::memory::malloc(sizeof(DevfsDirHandle));
    h->index = 0;
    return h;
}

int DevFS::read_dir(void *dir_handle, char *out_name, size_t max_len) {
    auto *h = (DevfsDirHandle *) dir_handle;
    if (h->index >= nodes->size()) return 0;
    strncpy(out_name, (*nodes)[h->index]->node->name, max_len - 1);
    out_name[max_len - 1] = '\0';
    h->index++;
    return 1;
}

void DevFS::close_dir(void *dir_handle) {
    kernel::memory::free(dir_handle);
}
