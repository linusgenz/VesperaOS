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

#include "../../include/vector.h"
#include "blockdevice.h"
#include "../sync/mutex.h"
#include "device_manager.h"

namespace kernel {
    Vector<BlockDevice *> *DeviceManager::devices;
    mutex_t DeviceManager::device_manager_mutex;

    void DeviceManager::Init() {
        devices = new Vector<BlockDevice *>();
        mutex_init(&device_manager_mutex);
        devices->clear();
    }

    void DeviceManager::AddDevice(BlockDevice *device) {
        mutex_lock(&device_manager_mutex);
        devices->push_back(device);
        mutex_unlock(&device_manager_mutex);
    }

    Vector<BlockDevice *> DeviceManager::GetDevices() {
        mutex_lock(&device_manager_mutex);
        auto copy = devices;
        mutex_unlock(&device_manager_mutex);
        return *copy;
    }

    uint32_t DeviceManager::GetDeviceCount() {
        mutex_lock(&device_manager_mutex);
        auto copy = devices->size();
        mutex_unlock(&device_manager_mutex);
        return copy;
    }
}
