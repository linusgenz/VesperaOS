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

#include <vector.h>

#include "../../filesystem/devfs/devfs.h"
#include "../../filesystem/partition/partition.h"
#include "../../include/kernel/devices/device_manager.h"
#include "blockdevice.h"
#include "log.h"
#include "partition_device.h"

Vector<BlockDevice*>* DeviceManager::devices;
Vector<KernelDevice*>* DeviceManager::all_devices;
uint32_t DeviceManager::next_id = 1;
spinlock_t DeviceManager::lock;

static bool blockLetterUsed[26] = {false};

void DeviceManager::init() {
    devices = new Vector<BlockDevice*>();
    all_devices = new Vector<KernelDevice*>();
    next_id = 1;
    lock.init();
    devices->clear();
    all_devices->clear();
}

char DeviceManager::GetNextFreeBlockLetter() {
    for (int i = 0; i < 26; i++) {
        if (!blockLetterUsed[i]) {
            blockLetterUsed[i] = true;
            return static_cast<char>('a' + i);
        }
    }
    return '?';
}

void DeviceManager::ReleaseBlockLetter(char c) {
    if (c >= 'a' && c <= 'z') {
        blockLetterUsed[c - 'a'] = false;
    }
}

char* DeviceManager::GenerateSDDeviceName(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 4) return nullptr;
    buffer[0] = 's';
    buffer[1] = 'd';
    buffer[2] = GetNextFreeBlockLetter();
    buffer[3] = '\0';
    return buffer;
}

char* DeviceManager::GenerateNVMeDeviceName(
    const KernelDevice* controller, char* buffer, size_t buffer_size, uint32_t namespaceId
) {
    if (!controller || !buffer || buffer_size < 16) return nullptr;

    if (controller->controller != ControllerType::NVMe) return nullptr;

    snprintf(buffer, buffer_size, "%sn%u", controller->name, namespaceId);

    return buffer;
}

size_t DeviceManager::FindAndRegisterPartitions(KernelDevice* physical_kd) {
    if (!physical_kd || !physical_kd->block) return 0;

    BlockDevice* dev = physical_kd->block;

    PartitionEntry parts[16];
    const size_t count = parse_partitions(dev, parts, 16);

    if (count == 0) return 0;

    for (size_t i = 0; i < count; ++i) {
        const PartitionEntry& pe = parts[i];

        auto* pdev = new PartitionDevice(dev, pe.start_lba, pe.length_lba);

        char part_name[64];
        if (physical_kd->controller == ControllerType::NVMe) {
            // physical_kd->name = "nvme0n1" → partition: "nvme0n1p1"
            snprintf(part_name, sizeof(part_name), "%sp%zu", physical_kd->name, i + 1);
        } else {
            // physical_kd->name = "sda" → partition: "sda1"
            snprintf(part_name, sizeof(part_name), "%s%zu", physical_kd->name, i + 1);
        }
        KernelDevice* pkd = RegisterBlockDevice(
            pdev, part_name, physical_kd->dev_class, physical_kd->bus_type, physical_kd->controller, physical_kd
        );

        DevFS::register_device(pkd);

        pkd->driver_data = nullptr;
    }

    return count;
}

bool DeviceManager::AllocUniqueDeviceName(const char* base, char* outBuffer, size_t outBufferSize) {
    spinlock_guard guard(lock);

    if (!all_devices || !outBuffer || outBufferSize == 0) return false;

    int counter = 0;

    while (true) {
        if (const int written = snprintf(outBuffer, outBufferSize, "%s%d", base, counter);
            written < 0 || static_cast<size_t>(written) >= outBufferSize)
            return false;

        bool exists = false;
        for (const auto* kd : *all_devices) {
            if (kd && kd->name && strcmp(kd->name, outBuffer) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) return true;

        counter++;
    }
}

Vector<BlockDevice*> DeviceManager::GetDevices() {
    spinlock_guard guard(lock);
    Vector<BlockDevice*> snapshot = devices->copy();
    return snapshot;
}

// 1-based -> 1 == 1 device, zero == 0 devices
uint32_t DeviceManager::GetDeviceCount() {
    spinlock_guard guard(lock);
    auto result = devices->size();
    return result;
}

KernelDevice* DeviceManager::RegisterBlockDevice(
    BlockDevice* dev, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent
) {
    if (!dev) return nullptr;

    spinlock_guard guard(lock);

    if (!all_devices) {
        all_devices = new Vector<KernelDevice*>();
    }
    if (!devices) {
        devices = new Vector<BlockDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id++;
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

    devices->push_back(dev);
    all_devices->push_back(kd);
    return kd;
}

KernelDevice* DeviceManager::RegisterCharDevice(
    CharDevice* dev, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent
) {
    if (!dev) return nullptr;

    spinlock_guard guard(lock);

    if (!all_devices) {
        all_devices = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id++;
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

    all_devices->push_back(kd);

    return kd;
}

KernelDevice* DeviceManager::RegisterController(
    const char* name, DeviceClass dev_class, BusType bus, ControllerType controller, KernelDevice* parent,
    ::CharDevice* dev, IDriverLifecycle* lifecycle
) {
    spinlock_guard guard(lock);

    if (!all_devices) {
        all_devices = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id++;
    kd->name = strdup(name);
    kd->type = DeviceType::Controller;
    kd->dev_class = dev_class;
    kd->controller = controller;
    kd->bus_type = bus;
    kd->parent = parent;
    kd->block = nullptr;
    kd->chardev = dev;
    kd->lifecycle = lifecycle;

    kd->driver_data = nullptr;

    if (parent) {
        parent->children.push_back(kd);
    }

    all_devices->push_back(kd);
    return kd;
}

KernelDevice* DeviceManager::RegisterGpuDevice(
    IRenderDriver* driver, const char* name, DeviceClass dev_class, BusType bus, ControllerType controller,
    KernelDevice* parent = nullptr
) {
    if (!driver) return nullptr;

    spinlock_guard guard(lock);

    if (!all_devices) {
        all_devices = new Vector<KernelDevice*>();
    }

    auto* kd = new KernelDevice();
    kd->id = next_id++;
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

    all_devices->push_back(kd);
    return kd;
}

void DeviceManager::UnregisterDevice(KernelDevice* kd) {
    if (!kd) return;

    spinlock_guard guard(lock);

    if (all_devices) {
        for (size_t i = 0; i < all_devices->size(); ++i) {
            if ((*all_devices)[i] == kd) {
                all_devices->erase(i);
                break;
            }
        }
    }

    if (kd->block && devices) {
        devices->erase_value(kd->block);
    }

    if (kd->parent) {
        auto& children = kd->parent->children;
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i] == kd) {
                children.erase(i);
                break;
            }
        }
    }

    if (kd->name) {
        if (kd->type == DeviceType::Block) {
            const char released_char = kd->name[3];
            ReleaseBlockLetter(released_char);
        }
        kernel::memory::free(const_cast<char*>(kd->name));
        kd->name = nullptr;
    }

    delete kd;
}

Vector<KernelDevice*> DeviceManager::GetAllDevices() {
    spinlock_guard guard(lock);
    if (!all_devices) {
        return Vector<KernelDevice*>();
    }
    return all_devices->copy();
}

KernelDevice* DeviceManager::FindById(uint32_t id) {
    spinlock_guard guard(lock);
    if (!all_devices) return nullptr;
    for (auto* dev : *all_devices) {
        if (dev && dev->id == id) {
            return dev;
        }
    }
    return nullptr;
}

uint32_t DeviceManager::GetKernelDeviceCount() {
    spinlock_guard guard(lock);
    if (!all_devices) return 0;
    return all_devices->size();
}

void DeviceManager::ShutdownAll() {
    spinlock_guard guard(lock);
    if (!all_devices) return;

    for (const auto* kd : *all_devices) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_shutdown();
    }
}

void DeviceManager::SuspendAll() {
    spinlock_guard guard(lock);
    if (!all_devices) return;

    for (const auto* kd : *all_devices) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_suspend();
    }
}

void DeviceManager::ResumeAll() {
    spinlock_guard guard(lock);
    if (!all_devices) return;

    for (const auto* kd : *all_devices) {
        if (!kd || !kd->lifecycle) continue;
        if (kd->type != DeviceType::Controller) continue;
        kd->lifecycle->on_resume();
    }
}