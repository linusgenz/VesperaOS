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

#include <klib/string.h>
#include <uapi/vespera/dev/ioctl_devinfo.h>
#include <uapi/vespera/dev/ioctl_smart.h>
#include <uapi/vespera/poll.h>
#include <vespera/devices/char_device.h>
#include <vespera/devices/device_info.h>
#include <vespera/filesystem/devfs.h>
#include <vespera/filesystem/vfs.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera_errno.h>

#include "../../include/vespera/cpu/io.h"
#include "../../kernel/graphics/display_manager.h"
#include "../drivers/ahci/ahci.h"
#include "uapi/vespera/dev/ioctl_usb_device.h"
#include "vespera/devices/usb_device_info.h"

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

VfsNodeType map_device_type(const DeviceType type) {
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
    ops_.truncate = nullptr;
    ops_.poll = poll;
    ops_.stat = stat;
}

int DevFs::register_device(KernelDevice* kd) {
    if (!kd || !kd->name) return -EINVAL;

    SpinlockGuard guard(lock_);

    if (!root_) return -ENOMEM;

    if (lookup_device(root_, kd->name)) return -EEXIST;

    auto* node = static_cast<VfsNode*>(kernel::memory::malloc(sizeof(VfsNode)));
    node->name = kd->name;
    node->type = map_device_type(kd->type);
    node->mount = nullptr;

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

Result<void> DevFs::open(const VfsNode* node) {
    if (!node) return Result<void>::err(Error::EINVAL);

    auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return Result<void>::err(Error::EINVAL);

    if (const KernelDevice* kd = entry->device; kd->chardev && !entry->cf) {
        CharFile* cf = nullptr;
        if (const int ret = kd->chardev->open(&cf); ret != 0) {
            return Result<void>::err(static_cast<Error>(ret));
        };

        entry->cf = cf;
    }

    return Result<void>::ok();
}

Result<usize> DevFs::read(const VfsNode* node, const usize offset, const usize size, void* buffer) {
    if (!node) return Result<usize>::err(Error::EINVAL);

    const auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return Result<usize>::err(Error::EINVAL);

    SpinlockGuard guard(lock_);

    const KernelDevice* kd = entry->device;
    if (kd->chardev) {
        if (!entry->cf) open(node);
        if (isize i = kd->chardev->read(entry->cf, buffer, size, offset); i != 0) {
            return Result<usize>::err(static_cast<Error>(i));
        } else {
            return Result<usize>::ok(i);
        }
    }
    if (kd->block) {
        const usize sector_size = kd->block->get_sector_size();
        const u64 lba = offset / sector_size;
        const u32 sectors = (size + sector_size - 1) / sector_size;
        if (isize i = kd->block->read(lba, sectors, buffer, size); i != 0) {
            return Result<usize>::err(static_cast<Error>(i));
        } else {
            return Result<usize>::ok(i);
        }
    }

    return Result<usize>::err(Error::EINVAL);
}

Result<usize> DevFs::write(VfsNode* node, const usize offset, const usize size, const void* buffer) {
    if (!node) return Result<usize>::err(Error::EINVAL);

    const auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return Result<usize>::err(Error::EINVAL);

    SpinlockGuard guard(lock_);

    const KernelDevice* kd = entry->device;

    // CharDevice
    if (kd->chardev) {
        if (!entry->cf) {
            if (const auto res = open(node); res.is_err()) return Result<usize>::err(res.err_code());
        }
        if (isize i = kd->chardev->write(entry->cf, buffer, size); i != 0) {
            return Result<usize>::err(static_cast<Error>(i));
        } else {
            return Result<usize>::ok(i);
        }
    }

    // BlockDevice
    if (kd->block) {
        const usize sector_size = kd->block->get_sector_size();
        const u64 lba = offset / sector_size;
        const u32 sectors = (size + sector_size - 1) / sector_size;
        if (isize i = kd->block->write(lba, sectors, const_cast<void*>(buffer), sizeof(buffer)); i != 0) {
            return Result<usize>::err(static_cast<Error>(i));
        } else {
            return Result<usize>::ok(i);
        }
    }

    return Result<usize>::err(Error::EINVAL);
}

isize DevFs::ioctl(const VfsNode* node, const u32 cmd, void* arg) {
    if (!node) return -EINVAL;

    const auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    SpinlockGuard guard(lock_);

    const KernelDevice* kd = entry->device;

    if (cmd >= IOCTL_DEVINFO_GET_ALL && cmd <= IOCTL_DEVINFO_GET_FW) {
        IDeviceInfo* info = kd->info;

        if (!info && kd->dev_class == DeviceClass::Pseudo && strcmp(kd->name, "gpu") == 0) {
            auto primary = DisplayManager::primary();
            if (primary.kd) info = primary.kd->info;
        }

        if (!info && kd->parent) info = kd->parent->info;

        switch (cmd) {
            case IOCTL_DEVINFO_GET_ALL: {
                if (!info || !arg) return info ? -EINVAL : -ENOTTY;
                auto* d = static_cast<devinfo_t*>(arg);
                info->get_model(d->model, sizeof(d->model));
                info->get_serial(d->serial, sizeof(d->serial));
                info->get_vendor(d->vendor, sizeof(d->vendor));
                info->get_firmware(d->firmware, sizeof(d->firmware));
                return 0;
            }
            case IOCTL_DEVINFO_GET_MODEL: {
                if (!info || !arg) return info ? -EINVAL : -ENOTTY;
                return info->get_model(static_cast<devinfo_string_t*>(arg)->value, 128) ? 0 : -EIO;
            }
            case IOCTL_DEVINFO_GET_SERIAL: {
                if (!info || !arg) return info ? -EINVAL : -ENOTTY;
                return info->get_serial(static_cast<devinfo_string_t*>(arg)->value, 128) ? 0 : -EIO;
            }
            case IOCTL_DEVINFO_GET_VENDOR: {
                if (!info || !arg) return info ? -EINVAL : -ENOTTY;
                return info->get_vendor(static_cast<devinfo_string_t*>(arg)->value, 128) ? 0 : -EIO;
            }
            case IOCTL_DEVINFO_GET_FW: {
                if (!info || !arg) return info ? -EINVAL : -ENOTTY;
                return info->get_firmware(static_cast<devinfo_string_t*>(arg)->value, 128) ? 0 : -EIO;
            }
            default:
                return -ENOTTY;
        }
    }

    if (cmd == IOCTL_USB_GET_DEVICE_INFO) {
        const IUsbDeviceInfo* usb_info = kd->usb_info;
        if (!usb_info && kd->parent) usb_info = kd->parent->usb_info;

        if (!usb_info) return -ENOTTY;
        if (!arg)      return -EINVAL;

        return usb_info->get_usb_device_info(static_cast<usb_device_info_t*>(arg)) ? 0 : -EIO;
    }

    // CharDevice
    if (kd->chardev) {
        if (!entry->cf) {
            if (const auto res = open(node); res.is_err()) return -static_cast<isize>(res.err_code());
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
                return smart->smart_read_data(static_cast<smart_raw*>(arg)->data) ? 0 : -EIO;
            }
            case IOCTL_SMART_GET_COMMON:
                if (!smart || !arg) return smart ? -EINVAL : -ENOTTY;
                return smart->smart_get_common(static_cast<smart_common*>(arg)) ? 0 : -EIO;

            case IOCTL_SMART_GET_NVME:
                if (!smart || !arg) return smart ? -EINVAL : -ENOTTY;
                return smart->smart_get_nvme(static_cast<smart_nvme*>(arg)) ? 0 : -ENOTTY;

            case IOCTL_SMART_GET_ATA:
                if (!smart || !arg) return smart ? -EINVAL : -ENOTTY;
                return smart->smart_get_ata(static_cast<smart_ata*>(arg)) ? 0 : -ENOTTY;
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

int DevFs::poll(const VfsNode* node) {
    if (!node) return -EINVAL;

    const auto* entry = static_cast<DevfsEntry*>(node->internal_data);
    if (!entry || !entry->device) return -EINVAL;

    const KernelDevice* kd = entry->device;

    if (kd->chardev) {
        if (!entry->cf) {
            if (const auto res = open(node); res.is_err()) return -static_cast<isize>(res.err_code());
        }
        return kd->chardev->poll(entry->cf);
    }

    if (kd->block) return POLLIN | POLLOUT;

    return POLLERR;
}