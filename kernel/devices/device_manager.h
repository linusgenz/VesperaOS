// device_manager.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "../../include/vector.h"
#include "blockdevice.h"
#include "../sync/mutex.h"

namespace kernel {

    class DeviceManager {
    public:
        static void Init();

        static void AddDevice(BlockDevice* device);
        static Vector<BlockDevice*> GetDevices();
        static uint32_t GetDeviceCount();

    private:
        static Vector<BlockDevice *> *devices;
        static mutex_t device_manager_mutex;
    };

}

#endif //DEVICE_MANAGER_H
