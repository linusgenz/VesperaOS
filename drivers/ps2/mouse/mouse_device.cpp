/**
 * @file mouse_device.cpp
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

#include "mouse_device.h"
#include "../ps2_controller.h"
#include "../../../filesystem/devfs/devfs.h"

Ps2MouseDevice::Ps2MouseDevice(Ps2Controller* controller)
    : CharDevice(BusType::Ps2), parent(controller)
{
    devnode = DeviceManager::register_char_device(
        this,
        "ps2mouse",
        DeviceClass::Input,
        BusType::Ps2,
        ControllerType::Ps2,
        parent->devnode
    );
    DevFs::register_device(devnode);
}

Ps2MouseDevice::~Ps2MouseDevice()
{
    DevFs::unregister_device(devnode);
    DeviceManager::unregister_device(devnode);
}


int Ps2MouseDevice::open(CharFile**) { return 0; }
int Ps2MouseDevice::release(CharFile*) { return 0; }

ssize_t Ps2MouseDevice::read(CharFile*, void* buf, size_t count, size_t)
{
   // return kernel::input::InputManager::read_mouse(buf, count);
   return 0;
}

ssize_t Ps2MouseDevice::write(CharFile*, const void*, size_t)
{
    return 0;
}
