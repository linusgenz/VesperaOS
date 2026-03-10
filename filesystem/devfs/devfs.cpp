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

#include <uapi/vespera/dev/ioctl_smart.h>
#include <vespera/devices/char_device.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera_errno.h>

#include "../../kernel/cpu/io.h"
#include "../drivers/ahci/ahci.h"
#include "../vfs/vfs.h"

const char* DevFs::bus_to_str(const BusType bus) {
    switch (bus) {
        case BusType::Usb:
            return "usb";
        case BusType::I2C:
            return "I2c";
        case BusType::Ps2:
            return "ps2";
        case BusType::Spi:
            return "spi";
        case BusType::Pci:
            return "pci";
        case BusType::Tty:
            return "tty";
        case BusType::VIRTUAL:
            return "virtual";
        default:
            return "unknown";
    }
}

VfsNodeType map_device_type(DeviceType type) {
    switch (type) {
        case DeviceType::Char:
            return VfsNodeType::CharDevice;
        case DeviceType::Block:
            return VfsNodeType::BlockDevice;
        default:
            return VfsNodeType::OtherDevice;
    }
}

void DevFs::init() {
    VirtualFilesystem::init("/dev", "dev");
    outb(0x3F8, 'F');
    ops_.read = read;
    ops_.write = write;
    ops_.find = find;
    ops_.close = close;
    ops_.opendir = open_dir;
    ops_.readdir = read_dir;
    ops_.closedir = close_dir;
    ops_.ioctl = ioctl;
    ops_.create = nullptr;
    ops_.rename = nullptr;
    ops_.mkdir = nullptr;
    ops_.rmdir = nullptr;
    ops_.unlink = nullptr;
}

int DevFs::register_device(KernelDevice* kd) {
    if (!kd || !kd->name) return -EINVAL;

    SpinlockGuard guard(lock_);

    if (!root_) return -ENOMEM;

    if (lookup_device(root_, kd->name)) return -EEXIST;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = kd->name;
    node->type = map_device_type(kd->type);

    node->ops = &ops_;
    node->permanent = false;

    auto* entry = static_cast<DevfsEntry*>(kernel::memory::malloc(sizeof(DevfsEntry)));
    entry->device = kd;
    entry->node = node;
    entry->is_directory = false;
    entry->cf = nullptr;

    node->internal_data = entry;
    kd->vfs_node_parent = root_;  // root as parent

    auto* root_data = static_cast<DirData*>(root_->internal_data);
    if (!root_data) {
        root_data = static_cast<DirData*>(kernel::memory::malloc(sizeof(DirData)));
        root_data->subdirs = Vector<VfsNode*>();
        root_data->files = Vector<VfsNode*>();
        root_->internal_data = root_data;
    }

    root_data->files.push_back(node);

    return SUCCESS_CODE;
}

int DevFs::unregister_device(KernelDevice* kd) {
    if (!kd) return -EINVAL;

    SpinlockGuard guard(lock_);

    VfsNode* node = kd->vfs_node_parent;
    if (!node) return -ENOENT;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry) return -ENOENT;

    auto* root_data = static_cast<DirData*>(node->internal_data);
    auto& devices = root_data->files;
    for (usize i = 0; i < devices.size(); ++i) {
        if (devices[i] == node) {
            devices.erase(i);
            break;
        }
    }

    kernel::memory::free(entry);
    kernel::memory::free(node);

    kd->vfs_node_parent = nullptr;

    return SUCCESS_CODE;
}

int DevFs::open(const VfsNode* node) {
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    if (const KernelDevice* kd = entry->device; kd->chardev && !entry->cf) {
        CharFile* cf = nullptr;
        if (const int ret = kd->chardev->open(&cf); ret != 0) return ret;

        entry->cf = cf;
    }

    return SUCCESS_CODE;
}

isize DevFs::read(const VfsNode* node, usize offset, usize size, void* buffer) {
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    SpinlockGuard guard(lock_);

    KernelDevice* kd = entry->device;
    if (kd->chardev) {
        if (!entry->cf) open(node);
        return kd->chardev->read(entry->cf, buffer, size, offset);
    }
    if (kd->block) {
        usize sector_size = kd->block->get_sector_size();
        u64 lba = offset / sector_size;
        u32 sectors = (size + sector_size - 1) / sector_size;
        return kd->block->read(lba, sectors, buffer, sizeof(buffer));
    }

    return -EINVAL;
}

isize DevFs::write(VfsNode* node, usize offset, const usize size, const void* buffer) {
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    SpinlockGuard guard(lock_);

    KernelDevice* kd = entry->device;

    // CharDevice
    if (kd->chardev) {
        if (!entry->cf) {
            if (const int res = open(node); res < 0) return res;
        }
        return kd->chardev->write(entry->cf, buffer, size);
    }

    // BlockDevice
    if (kd->block) {
        usize sector_size = kd->block->get_sector_size();
        u64 lba = offset / sector_size;
        u32 sectors = (size + sector_size - 1) / sector_size;
        return kd->block->write(lba, sectors, const_cast<void*>(buffer), sizeof(buffer));
    }

    return -EINVAL;
}

isize DevFs::ioctl(const VfsNode* node, const u32 cmd, void* arg) {
    if (!node) return -EINVAL;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    SpinlockGuard guard(lock_);

    KernelDevice* kd = entry->device;

    // CharDevice
    if (kd->chardev) {
        if (!entry->cf) {
            if (const int res = open(node); res < 0) return res;
        }
        return kd->chardev->ioctl(entry->cf, cmd, arg);
    }

    if (kd->block) {
        ISmartDevice* smart = kd->smart;
        if (!smart && kd->parent) {
            smart = kd->parent->smart;
        }

        switch (cmd) {
            case IOCTL_SMART_GET_RAW: {
                if (!smart) return -ENOTTY;
                if (!arg) return -EINVAL;
                return smart->smart_read_data(static_cast<SmartRawData*>(arg)->data) ? 0 : -EIO;
            }
            case IOCTL_SMART_GET_ATTRS: {
                if (!smart) return -ENOTTY;
                if (!arg) return -EINVAL;
                return smart->smart_get_attributes(static_cast<SmartAttributes*>(arg)) ? 0 : -EIO;
            }
            default:
                return -ENOTTY;
        }
    }

    return -EINVAL;
}

void DevFs::close(VfsNode* node) {
    if (!node) return;

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device || !entry->cf) return;

    SpinlockGuard guard(lock_);

    if (const KernelDevice* kd = entry->device; kd->chardev) kd->chardev->release(entry->cf);

    entry->cf = nullptr;
}
