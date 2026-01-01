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
#include "../../include/kernel/devices/char_device.h"
#include "../vfs/vfs.h"

const char* DevFS::bus_to_str(const BusType bus)
{
    switch (bus)
    {
    case BusType::BUS_USB: return "usb";
    case BusType::BUS_I2C: return "i2c";
    case BusType::BUS_PS2: return "ps2";
    case BusType::BUS_SPI: return "spi";
    case BusType::BUS_PCI: return "pci";
    case BusType::BUS_TTY: return "tty";
    case BusType::VIRTUAL: return "virtual";
    default: return "unknown";
    }
}

VfsNodeType mapDeviceType(DeviceType type)
{
    switch (type)
    {
    case DeviceType::Char:
        return VfsNodeType::CharDevice;
    case DeviceType::Block:
        return VfsNodeType::BlockDevice;
    default:
        return VfsNodeType::OtherDevice;
    }
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

int DevFS::register_device(KernelDevice* kd)
{
    if (!kd || !kd->name) return -EINVAL;

    spinlock_guard guard(lock);

    VfsNode* root_dir = root;
    if (!root_dir) return -ENOMEM;

    if (lookup_device(root_dir, kd->name)) return -EEXIST;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = kd->name;
    node->type = mapDeviceType(kd->type);

    node->ops = &ops;
    node->permanent = false;

    auto* entry = static_cast<DevfsEntry*>(kernel::memory::malloc(sizeof(DevfsEntry)));
    entry->device = kd;
    entry->node = node;
    entry->is_directory = false;
    entry->cf = nullptr;

    node->internal_data = entry;
    kd->vfs_node_parent = root_dir; // root as parent

    auto* root_data = static_cast<DirData*>(root_dir->internal_data);
    if (!root_data)
    {
        root_data = static_cast<DirData*>(kernel::memory::malloc(sizeof(DirData)));
        root_data->subdirs = Vector<VfsNode*>();
        root_data->files = Vector<VfsNode*>();
        root_dir->internal_data = root_data;
    }

    root_data->files.push_back(node);

    return SUCCESS_CODE;
}

int DevFS::unregister_device(KernelDevice* kd)
{
    if (!kd) return -EINVAL;

    spinlock_guard guard(lock);

    VfsNode* node = kd->vfs_node_parent;
    if (!node) return -ENOENT;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry) return -ENOENT;

    auto* root_data = static_cast<DirData*>(node->internal_data);
    auto& devices = root_data->files;
    for (size_t i = 0; i < devices.size(); ++i)
    {
        if (devices[i] == node)
        {
            devices.erase(i);
            break;
        }
    }

    kernel::memory::free(entry);
    kernel::memory::free(node);

    kd->vfs_node_parent = nullptr;

    return SUCCESS_CODE;
}

int DevFS::open(const VfsNode* node)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    KernelDevice* kd = entry->device;

    if (kd->chardev && !entry->cf)
    {
        CharFile* cf = nullptr;
        int ret = kd->chardev->open(&cf);
        if (ret != 0)
            return ret;

        entry->cf = cf;
    }

    return SUCCESS_CODE;
}


ssize_t DevFS::read(const VfsNode* node, size_t offset, size_t size, void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    KernelDevice* kd = entry->device;
    if (kd->chardev)
    {
        if (!entry->cf) open(node);
        return kd->chardev->read(entry->cf, buffer, size, offset);
    }
    if (kd->block)
    {
        size_t sector_size = kd->block->get_sector_size();
        uint64_t lba = offset / sector_size;
        uint32_t sectors = (size + sector_size - 1) / sector_size;
        return kd->block->read(lba, sectors, buffer, sizeof(buffer));
    }

    return -EINVAL;
}


ssize_t DevFS::write(VfsNode* node, size_t offset, const size_t size, const void* buffer)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    KernelDevice* kd = entry->device;

    // CharDevice
    if (kd->chardev)
    {
        if (!entry->cf)
        {
            int res = open(node);
            if (res < 0) return res;
        }
        return kd->chardev->write(entry->cf, buffer, size);
    }

    // BlockDevice
    if (kd->block)
    {
        size_t sector_size = kd->block->get_sector_size();
        uint64_t lba = offset / sector_size;
        uint32_t sectors = (size + sector_size - 1) / sector_size;
        return kd->block->write(lba, sectors, const_cast<void*>(buffer), sizeof(buffer));
    }

    return -EINVAL;
}

ssize_t DevFS::ioctl(const VfsNode* node, const uint32_t cmd, void* arg)
{
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    spinlock_guard guard(lock);

    KernelDevice* kd = entry->device;

    // CharDevice
    if (kd->chardev)
    {
        if (!entry->cf)
        {
            int res = open(node);
            if (res < 0) return res;
        }
        return kd->chardev->ioctl(entry->cf, cmd, arg);
    }

    // Block device does not support ioctl (maybe implement later necessary)
    if (kd->block)
    {
        return -ENOTTY;
    }

    return -EINVAL;
}

void DevFS::close(VfsNode* node)
{
    if (!node) return;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device || !entry->cf) return;

    spinlock_guard guard(lock);

    KernelDevice* kd = entry->device;
    if (kd->chardev)
        kd->chardev->release(entry->cf);

    entry->cf = nullptr;
}
