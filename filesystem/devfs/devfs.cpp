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

#include "log.h"
#include "../../kernel/devices/chardevice.h"
#include "../vfs/vfs.h"

const char* DevFS::bus_to_str(const BusType bus)
{
    switch (bus)
    {
    case BUS_XHCI: return "xhci";
    case BUS_I2C: return "i2c";
    case BUS_SPI: return "spi";
    case BUS_PCI: return "pci";
    case BUS_TTY: return "tty";
    case VIRTUAL: return "virtual";
    default: return "unknown";
    }
}

VfsNode* DevFS::ensure_bus_dir(const BusType bus)
{
    const char* bus_name = bus_to_str(bus);

    if (bus == BUS_NONE || bus == VIRTUAL) return root;

    return ensure_subdirectory(bus_name, root);;
}

void DevFS::init()
{
    VirtualFilesystem::init("/dev", "dev");

    ops.read = read;
    ops.write = write;
    ops.find = find;
    ops.close = close;
    ops.opendir = open_dir;
    ops.readdir = read_dir;
    ops.closedir = close_dir;
    ops.ioctl = ioctl;
    ops.create = nullptr;
    ops.rename = nullptr;
    ops.mkdir = nullptr;
    ops.rmdir = nullptr;
    ops.unlink = nullptr;
}

int DevFS::register_device(CharDevice* dev)
{
    if (!dev || strlen(dev->name) == 0) return -EINVAL;
    spinlock_guard guard(lock);

    VfsNode* bus_dir = ensure_bus_dir(dev->bus_type);
    if (!bus_dir) return -ENOMEM;

    if (lookup_device(bus_dir, dev->name)) return -EEXIST;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = dev->name;
    node->type = VfsNodeType::Device;
    node->ops = &ops;
    node->permanent = false;

    auto* entry = static_cast<DevfsEntry*>(kernel::memory::malloc(sizeof(DevfsEntry)));
    memset(entry, 0, sizeof(DevfsEntry));
    entry->device = dev;
    entry->node = node;
    entry->is_directory = false;
    entry->cf = nullptr;

    dev->parent = bus_dir;

    node->internal_data = entry;

    auto* bus_data = static_cast<DirData*>(bus_dir->internal_data);
    if (!bus_data)
    {
        bus_data = static_cast<DirData*>(kernel::memory::malloc(sizeof(DirData)));
        bus_data->subdirs = Vector<VfsNode*>();
        bus_data->files = Vector<VfsNode*>();
        bus_dir->internal_data = bus_data;
    }
    bus_data->files.push_back(node);

    return SUCCESS_CODE;
}

int DevFS::unregister_device(CharDevice* dev)
{
    spinlock_guard guard(lock);

    VfsNode* node = dev->parent;
    if (!node) return -ENOENT;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry) return -ENOENT;

    // Speicher freigeben
    if (auto* parent_data = static_cast<DirData*>(entry->node->internal_data))
    {
        auto& devices = parent_data->files;
        for (size_t i = 0; i < devices.size(); i++)
        {
            if (devices[i] == node)
            {
                devices.erase(i);
                break;
            }
        }
    }

    kernel::memory::free(entry->device);
    kernel::memory::free(entry);
    kernel::memory::free(node);

    return SUCCESS_CODE;
}

const char* DevFS::alloc_unique_name(const char* base, BusType type)
{
    spinlock_guard guard(lock);

    static char buffer[64];
    int counter = 0;

    VfsNode* bus_dir = ensure_bus_dir(type);
    if (!bus_dir) return nullptr;

    while (true)
    {
        snprintf(buffer, sizeof(buffer), "%s%d", base, counter);

        if (!lookup_device(bus_dir, buffer)) break;
        counter++;
    }

    const size_t len = strlen(buffer) + 1;
    auto result = static_cast<char*>(kernel::memory::malloc(len));
    strncpy(result, buffer, len);
    return result;
}

int DevFS::open(const VfsNode* node)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    if (!entry->cf)
    {
        entry->cf = new CharFile();
        entry->cf->driver_private = nullptr;
        return entry->device->open(&entry->cf);
    }

    return SUCCESS_CODE;
}

ssize_t DevFS::read(const VfsNode* node, const size_t offset, const size_t size, void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    if (!entry->cf)
    {
        int result = open(node);
        if (result < 0) return result;
    }

    return entry->device->read(entry->cf, buffer, size, offset);
}

ssize_t DevFS::write(VfsNode* node, size_t offset, const size_t size, const void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    if (!entry->cf)
    {
        int result = open(node);
        if (result < 0) return result;
    }

    return entry->device->write(entry->cf, buffer, size);
}

ssize_t DevFS::ioctl(const VfsNode* node, const uint32_t cmd, void* arg)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    if (!entry->cf)
    {
        int result = open(node);
        if (result < 0) return result;
    }

    return entry->device->ioctl(entry->cf, cmd, arg);
}

void DevFS::close(VfsNode* node)
{
    if (!node) return;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device || !entry->cf) return;

    spinlock_guard guard(lock);
    entry->device->release(entry->cf);
    delete entry->cf;
    entry->cf = nullptr;
}
