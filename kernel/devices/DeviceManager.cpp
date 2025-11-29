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
#include "device_manager.h"

namespace kernel {
    Vector<BlockDevice *> *DeviceManager::devices;
    spinlock_t DeviceManager::lock;

    void DeviceManager::Init() {
        devices = new Vector<BlockDevice *>();
        lock.init();
        devices->clear();
    }

    void DeviceManager::AddDevice(BlockDevice *device) {
        spinlock_guard guard(lock);
        devices->push_back(device);
    }

    void DeviceManager::RemoveDevice(BlockDevice *device) {
        spinlock_guard guard(lock);
        devices->erase_value(device);
    }

    Vector<BlockDevice *> DeviceManager::GetDevices() {
        spinlock_guard guard(lock);
        Vector<BlockDevice *> snapshot = devices->copy();
        return snapshot;
    }


    // 1-based -> 1 == 1 device, zero == 0 devices
    uint32_t DeviceManager::GetDeviceCount() {
        spinlock_guard guard(lock);
        auto result = devices->size();
        return result;
    }
}
