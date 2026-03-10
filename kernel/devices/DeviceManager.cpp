// DeviceManager.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.08.25.
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

#include "../../filesystem/devfs/devfs.h"
#include "../../filesystem/partition/partition.h"
#include <vespera/devices/block.h>
#include <vespera/devices/device_manager.h>
#include <vespera/log.h>
#include "partition_device.h"
#include <klib/vector.h>

Vector<BlockDevice*>* DeviceManager::devices_;
Vector<KernelDevice*>* DeviceManager::all_devices_;
u32 DeviceManager::next_id_ = 1;
Spinlock DeviceManager::lock_;

static bool block_letter_used[26] = {false};

void DeviceManager::init() {
    devices_ = new Vector<BlockDevice*>();
    all_devices_ = new Vector<KernelDevice*>();
    next_id_ = 1;
    lock_.init();
    devices_->clear();
    all_devices_->clear();
}

char DeviceManager::get_next_free_block_letter() {
    for (int i = 0; i < 26; i++) {
        if (!block_letter_used[i]) {
            block_letter_used[i] = true;
            return static_cast<char>('a' + i);
        }
    }
    return '?';
}

void DeviceManager::release_block_letter(char c) {
    if (c >= 'a' && c <= 'z') {
        block_letter_used[c - 'a'] = false;
    }
}

char* DeviceManager::generate_sd_device_name(char* buffer, usize buffer_size) {
    if (!buffer || buffer_size < 4) return nullptr;
    buffer[0] = 's';
    buffer[1] = 'd';
    buffer[2] = get_next_free_block_letter();
    buffer[3] = '\0';
    return buffer;
}

char* DeviceManager::generate_nv_me_device_name(
    const KernelDevice* controller, char* buffer, usize buffer_size, const u32 namespace_id
) {
    if (!controller || !buffer || buffer_size < 16) return nullptr;

    if (controller->controller != ControllerType::Nvme) return nullptr;

    snprintf(buffer, buffer_size, "%sn%u", controller->name, namespace_id);

    return buffer;
}

usize DeviceManager::find_and_register_partitions(KernelDevice* physical_kd) {
    if (!physical_kd || !physical_kd->block) return 0;

    BlockDevice* dev = physical_kd->block;

    PartitionEntry parts[16];
    const usize count = parse_partitions(dev, parts, 16);

    if (count == 0) return 0;

    for (usize i = 0; i < count; ++i) {
        const PartitionEntry& pe = parts[i];

        auto* pdev = new PartitionDevice(dev, pe.start_lba, pe.length_lba);

        char part_name[64];
        if (physical_kd->controller == ControllerType::Nvme) {
            // physical_kd->name = "nvme0n1" → partition: "nvme0n1p1"
            snprintf(part_name, sizeof(part_name), "%sp%zu", physical_kd->name, i + 1);
        } else {
            // physical_kd->name = "sda" → partition: "sda1"
            snprintf(part_name, sizeof(part_name), "%s%zu", physical_kd->name, i + 1);
        }
        KernelDevice* pkd = register_block_device(
            pdev, part_name, physical_kd->dev_class, physical_kd->bus_type, physical_kd->controller, physical_kd
        );

        DevFs::register_device(pkd);

        pkd->driver_data = nullptr;
    }

    return count;
}

bool DeviceManager::alloc_unique_device_name(const char* base, char* out_buffer, const usize out_buffer_size) {
    SpinlockGuard guard(lock_);

    if (!all_devices_ || !out_buffer || out_buffer_size == 0) return false;

    int counter = 0;

    while (true) {
        if (const int written = snprintf(out_buffer, out_buffer_size, "%s%d", base, counter);
            written < 0 || static_cast<usize>(written) >= out_buffer_size)
            return false;

        bool exists = false;
        for (const auto* kd : *all_devices_) {
            if (kd && kd->name && strcmp(kd->name, out_buffer) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) return true;

        counter++;
    }
}

Vector<BlockDevice*> DeviceManager::get_devices() {
    SpinlockGuard guard(lock_);
    Vector<BlockDevice*> snapshot = devices_->copy();
    return snapshot;
}

// 1-based -> 1 == 1 device, zero == 0 devices
u32 DeviceManager::get_device_count() {
    SpinlockGuard guard(lock_);
    auto result = devices_->size();
    return result;
}

KernelDevice* DeviceManager::register_block_device(
    BlockDevice* dev, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent, ISmartDevice* smart
) {
    if (!dev) return nullptr;

    SpinlockGuard guard(lock_);

    if (!all_devices_) {
        all_devices_ = new Vector<KernelDevice*>();
    }
    if (!devices_) {
        devices_ = new Vector<BlockDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id_++;
    kd->name = strdup(name);
    kd->type = DeviceType::Block;
    kd->dev_class = dev_class;
    kd->controller = controller;
    kd->bus_type = bus;
    kd->parent = parent;
    kd->block = dev;
    kd->chardev = nullptr;
    kd->driver_data = nullptr;

    if (parent) {
        parent->children.push_back(kd);
    }

    devices_->push_back(dev);
    all_devices_->push_back(kd);
    return kd;
}

KernelDevice* DeviceManager::register_char_device(
    CharDevice* dev, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent
) {
    if (!dev) return nullptr;

    SpinlockGuard guard(lock_);

    if (!all_devices_) {
        all_devices_ = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id_++;
    kd->name = strdup(name);
    kd->type = DeviceType::Char;
    kd->dev_class = dev_class;
    kd->controller = controller;
    kd->bus_type = bus;
    kd->parent = parent;
    kd->block = nullptr;
    kd->chardev = dev;
    kd->driver_data = nullptr;

    if (parent) {
        parent->children.push_back(kd);
    }

    all_devices_->push_back(kd);

    return kd;
}

KernelDevice* DeviceManager::register_controller(
    const char* name, DeviceClass dev_class, BusType bus, ControllerType controller, KernelDevice* parent,
    ::CharDevice* dev, IDriverLifecycle* lifecycle, ISmartDevice* smart
) {
    SpinlockGuard guard(lock_);

    if (!all_devices_) {
        all_devices_ = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id_++;
    kd->name = strdup(name);
    kd->type = DeviceType::Controller;
    kd->dev_class = dev_class;
    kd->controller = controller;
    kd->bus_type = bus;
    kd->parent = parent;
    kd->block = nullptr;
    kd->chardev = dev;
    kd->lifecycle = lifecycle;
    kd->smart = smart;

    kd->driver_data = nullptr;

    if (parent) {
        parent->children.push_back(kd);
    }

    all_devices_->push_back(kd);
    return kd;
}

KernelDevice* DeviceManager::register_gpu_device(
    IRenderDriver* driver, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent = nullptr
) {
    if (!driver) return nullptr;

    SpinlockGuard guard(lock_);

    if (!all_devices_) {
        all_devices_ = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id_++;
    kd->name = strdup(name);
    kd->type = DeviceType::Gpu;
    kd->dev_class = dev_class;
    kd->controller = controller;
    kd->bus_type = bus;
    kd->parent = parent;
    kd->block = nullptr;
    kd->chardev = nullptr;
    kd->driver_data = driver;

    if (parent) {
        parent->children.push_back(kd);
    }

    all_devices_->push_back(kd);
    return kd;
}

void DeviceManager::unregister_device(KernelDevice* kd) {
    if (!kd) return;

    SpinlockGuard guard(lock_);

    if (all_devices_) {
        for (usize i = 0; i < all_devices_->size(); ++i) {
            if ((*all_devices_)[i] == kd) {
                all_devices_->erase(i);
                break;
            }
        }
    }

    if (kd->block && devices_) {
        devices_->erase_value(kd->block);
    }

    if (kd->parent) {
        auto& children = kd->parent->children;
        for (usize i = 0; i < children.size(); ++i) {
            if (children[i] == kd) {
                children.erase(i);
                break;
            }
        }
    }

    if (kd->name) {
        if (kd->type == DeviceType::Block) {
            const char released_char = kd->name[3];
            release_block_letter(released_char);
        }
        kernel::memory::free(const_cast<char*>(kd->name));
        kd->name = nullptr;
    }

    delete kd;
}

Vector<KernelDevice*> DeviceManager::get_all_devices() {
    SpinlockGuard guard(lock_);
    if (!all_devices_) {
        return Vector<KernelDevice*>();
    }
    return all_devices_->copy();
}

KernelDevice* DeviceManager::find_by_id(u32 id) {
    SpinlockGuard guard(lock_);
    if (!all_devices_) return nullptr;
    for (auto* dev : *all_devices_) {
        if (dev && dev->id == id) {
            return dev;
        }
    }
    return nullptr;
}

u32 DeviceManager::get_kernel_device_count() {
    SpinlockGuard guard(lock_);
    if (!all_devices_) return 0;
    return all_devices_->size();
}

void DeviceManager::shutdown_all() {
    SpinlockGuard guard(lock_);
    if (!all_devices_) return;

    for (const auto* kd : *all_devices_) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_shutdown();
    }
}

void DeviceManager::suspend_all() {
    SpinlockGuard guard(lock_);
    if (!all_devices_) return;

    for (const auto* kd : *all_devices_) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_suspend();
    }
}

void DeviceManager::resume_all() {
    SpinlockGuard guard(lock_);
    if (!all_devices_) return;

    for (const auto* kd : *all_devices_) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_resume();
    }
}