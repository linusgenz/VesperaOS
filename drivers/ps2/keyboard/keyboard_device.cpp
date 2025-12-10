/**
 * @file keyboard_device.cpp
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

#include "keyboard_device.h"

#include "../ps2_controller.h"
#include <kernel/devices/device_manager.h>
#include <kernel/devices/char_device.h>

#include "../../../filesystem/devfs/devfs.h"

Ps2KeyboardDevice::Ps2KeyboardDevice(Ps2Controller* controller)
    : CharDevice("ps2kbd", BUS_PS2), parent(controller)
{
    devnode = DeviceManager::RegisterCharDevice(
        this,
        "ps2kbd",
        DeviceClass::Input,
        BUS_PS2,
        ControllerType::PS2,
        parent->devnode
    );
    DevFS::register_device(devnode);
}

Ps2KeyboardDevice::~Ps2KeyboardDevice()
{
    DevFS::unregister_device(devnode);
    DeviceManager::UnregisterDevice(devnode);
}


int Ps2KeyboardDevice::open(CharFile** out_cf) { return 0; }
int Ps2KeyboardDevice::release(CharFile*) { return 0; }

ssize_t Ps2KeyboardDevice::read(CharFile*, void* buffer, size_t count, size_t)
{
    //return kernel::input::InputManager::read_keyboard(buffer, count);
    return 0;
}

ssize_t Ps2KeyboardDevice::write(CharFile*, const void*, size_t)
{
    return 0;
}