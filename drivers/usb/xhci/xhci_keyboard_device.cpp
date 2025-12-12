/**
 * @file xhci_keyboard_devic.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 10.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "xhci.h"
#include "xhci_keyboard_device.h"
#include "../../../filesystem/devfs/devfs.h"
#include "../../../include/kernel/memory.h"
#include "../../../include/kernel/devices/device_manager.h"

UsbKeyboardDevice::UsbKeyboardDevice(const char* name, KernelDevice* parent)
    : CharDevice(name, BusType::BUS_USB)
{
    devnode = DeviceManager::RegisterCharDevice(
        this,
        name,
        DeviceClass::Input,
        BusType::BUS_USB,
        ControllerType::XHCI,
        parent
    );
    DevFS::register_device(devnode);
}

UsbKeyboardDevice::~UsbKeyboardDevice()
{
    DevFS::unregister_device(devnode);
    DeviceManager::UnregisterDevice(devnode);
}

int UsbKeyboardDevice::open(CharFile**) { return 0; }

int UsbKeyboardDevice::release(CharFile*) { return 0; }

ssize_t UsbKeyboardDevice::read(CharFile* cf, void* buffer, size_t count, size_t offset) { return 0; }

ssize_t UsbKeyboardDevice::write(CharFile* cf, const void* buffer, size_t count) { return 0; }
