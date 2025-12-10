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
#include "blockdevice.h"
#include "partition_device.h"
#include "../../filesystem/devfs/devfs.h"
#include "../../filesystem/partition/partition.h"
#include "../../include/kernel/devices/device_manager.h"


Vector<BlockDevice*>* DeviceManager::devices;
Vector<KernelDevice*>* DeviceManager::all_devices;
uint32_t DeviceManager::next_id = 1;
spinlock_t DeviceManager::lock;

void DeviceManager::Init()
{
    devices = new Vector<BlockDevice*>();
    all_devices = new Vector<KernelDevice*>();
    next_id = 1;
    lock.init();
    devices->clear();
    all_devices->clear();
}

char DeviceManager::GetNextFreeBlockLetter(KernelDevice* controller)
{
    if (!controller) return '?';
    for (int i = 0; i < 26; i++)
    {
        if (!controller->used_letters[i])
        {
            controller->used_letters[i] = true;
            return 'a' + i;
        }
    }
    return '?';
}

char* DeviceManager::GenerateSDDeviceName(KernelDevice* controller, char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 4) return nullptr;
    buffer[0] = 's';
    buffer[1] = 'd';
    buffer[2] = GetNextFreeBlockLetter(controller);
    buffer[3] = '\0';
    return buffer;
}

char* DeviceManager::GenerateNVMeDeviceName(KernelDevice* controller, char* buffer, size_t buffer_size)
{
    if (!buffer || buffer_size < 16) return nullptr;

    uint32_t dev_index = 0;
    // finde freien Slot
    for (size_t i = 0; i < controller->nvme_device_used.size(); i++)
    {
        if (!controller->nvme_device_used[i])
        {
            dev_index = i;
            controller->nvme_device_used[i] = true;
            break;
        }
    }

    // erweitere Vektor falls nötig
    if (dev_index >= controller->nvme_device_used.size())
    {
        controller->nvme_device_used.push_back(true);
    }

    snprintf(buffer, buffer_size, "nvme%dn%d", controller->next_nvme_index, dev_index + 1);
    return buffer;
}

size_t DeviceManager::FindAndRegisterPartitions(KernelDevice* physical_kd)
{
    if (!physical_kd || !physical_kd->block)
        return 0;

    BlockDevice* dev = physical_kd->block;

    PartitionEntry parts[16];
    const size_t count = parse_partitions(dev, parts, 16);

    if (count == 0)
        return 0;

    for (size_t i = 0; i < count; ++i)
    {
        const PartitionEntry& pe = parts[i];

        auto* pdev = new PartitionDevice(dev, pe.start_lba, pe.length_lba);

        // physical_kd->name = "sda" → partition: "sda1"
        char part_name[64];
        snprintf(part_name, sizeof(part_name), "%s%zu", physical_kd->name, i + 1);

        KernelDevice* pkd = RegisterBlockDevice(
            pdev,
            part_name,
            physical_kd->dev_class,
            physical_kd->bus_type,
            physical_kd->controller,
            physical_kd
        );

        DevFS::register_device(pkd);

        pkd->driver_data = nullptr;
    }

    return count;
}

const char* DeviceManager::AllocUniqueDeviceName(const char* base)
{
    spinlock_guard guard(lock);

    static char buffer[64];
    int counter = 0;

    if (!all_devices) return nullptr;

    while (true)
    {
        snprintf(buffer, sizeof(buffer), "%s%d", base, counter);

        bool exists = false;
        for (auto* kd : *all_devices)
        {
            if (kd && kd->name && strcmp(kd->name, buffer) == 0)
            {
                exists = true;
                break;
            }
        }

        if (!exists) break;
        counter++;
    }

    const size_t len = strlen(buffer) + 1;
    auto result = static_cast<char*>(kernel::memory::malloc(len));
    strncpy(result, buffer, len);
    result[len - 1] = '\0';
    return result;
}

Vector<BlockDevice*> DeviceManager::GetDevices()
{
    spinlock_guard guard(lock);
    Vector<BlockDevice*> snapshot = devices->copy();
    return snapshot;
}

// 1-based -> 1 == 1 device, zero == 0 devices
uint32_t DeviceManager::GetDeviceCount()
{
    spinlock_guard guard(lock);
    auto result = devices->size();
    return result;
}

KernelDevice* DeviceManager::RegisterBlockDevice(BlockDevice* dev,
                                                 const char* name,
                                                 DeviceClass dev_class,
                                                 BusType bus,
                                                 ControllerType controller,
                                                 KernelDevice* parent)
{
    if (!dev) return nullptr;

    spinlock_guard guard(lock);

    if (!all_devices)
    {
        all_devices = new Vector<KernelDevice*>();
    }
    if (!devices)
    {
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

    if (parent)
    {
        parent->children.push_back(kd);
    }

    devices->push_back(dev);
    all_devices->push_back(kd);
    return kd;
}

KernelDevice* DeviceManager::RegisterCharDevice(::CharDevice* dev,
                                                const char* name,
                                                DeviceClass dev_class,
                                                BusType bus,
                                                ControllerType controller,
                                                KernelDevice* parent)
{
    if (!dev) return nullptr;

    spinlock_guard guard(lock);

    if (!all_devices)
    {
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

    if (parent)
    {
        parent->children.push_back(kd);
    }

    all_devices->push_back(kd);

    return kd;
}

KernelDevice* DeviceManager::RegisterController(
    const char* name,
    DeviceClass dev_class,
    BusType bus,
    ControllerType controller,
    KernelDevice* parent, ::CharDevice* dev)
{
    spinlock_guard guard(lock);

    if (!all_devices)
    {
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
    kd->driver_data = nullptr;

    if (parent)
    {
        parent->children.push_back(kd);
    }

    all_devices->push_back(kd);
    return kd;
}

void DeviceManager::UnregisterDevice(KernelDevice* kd)
{
    if (!kd) return;

    spinlock_guard guard(lock);

    // Entferne aus all_devices
    if (all_devices)
    {
        for (size_t i = 0; i < all_devices->size(); ++i)
        {
            if ((*all_devices)[i] == kd)
            {
                all_devices->erase(i);
                break;
            }
        }
    }

    if (kd->block && devices)
    {
        devices->erase_value(kd->block);
    }

    if (kd->parent)
    {
        auto& children = kd->parent->children;
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (children[i] == kd)
            {
                children.erase(i);
                break;
            }
        }
    }

    if (kd->name)
    {
        kernel::memory::free(const_cast<char*>(kd->name));
        kd->name = nullptr;
    }

    delete kd;
}

Vector<KernelDevice*> DeviceManager::GetAllDevices()
{
    spinlock_guard guard(lock);
    if (!all_devices)
    {
        return Vector<KernelDevice*>();
    }
    return all_devices->copy();
}

KernelDevice* DeviceManager::FindById(uint32_t id)
{
    spinlock_guard guard(lock);
    if (!all_devices) return nullptr;
    for (auto* dev : *all_devices)
    {
        if (dev && dev->id == id)
        {
            return dev;
        }
    }
    return nullptr;
}

uint32_t DeviceManager::GetKernelDeviceCount()
{
    spinlock_guard guard(lock);
    if (!all_devices) return 0;
    return all_devices->size();
}
