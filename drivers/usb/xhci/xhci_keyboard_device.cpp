/**
 * @file xhci_keyboard_device.cpp
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

#include "xhci_keyboard_device.h"

#include <vespera/devices/device_manager.h>

#include <filesystem/devfs.h>
#include "xhci.h"

UsbKeyboardDevice::UsbKeyboardDevice(const char* name, KernelDevice* parent, UsbDeviceInfo* info)
    : CharDevice(BusType::Usb) {
    kd = DeviceManager::register_device(
        DeviceDescriptor{}
            .set_name(name)
            .set_type(DeviceType::Char)
            .set_class(DeviceClass::Input)
            .with_char(this)
            .set_bus(BusType::Usb)
            .set_controller(ControllerType::Xhci)
            .with_info(info)
            .with_usb_info(info)

    );
    DevFs::register_device(kd);
}

UsbKeyboardDevice::~UsbKeyboardDevice() {
    DevFs::unregister_device(kd);
    DeviceManager::unregister_device(kd);
}

int UsbKeyboardDevice::open(CharFile**) {
    return 0;
}

int UsbKeyboardDevice::release(CharFile*) {
    return 0;
}

isize UsbKeyboardDevice::read(CharFile* cf, void* buffer, usize count, usize offset) {
    return 0;
}

isize UsbKeyboardDevice::write(CharFile* cf, const void* buffer, usize count) {
    return 0;
}
