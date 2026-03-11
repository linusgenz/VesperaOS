/**
 * @file ps2_controller.cpp
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

#include "ps2_controller.h"

#include "vespera/devices/device_manager.h"

Ps2Controller::Ps2Controller()
{
    devnode = DeviceManager::register_device(
    DeviceDescriptor{}
        .set_name("i8042")
        .set_type(DeviceType::Controller)
        .set_class(DeviceClass::Misc)
        .set_bus(BusType::Ps2)
        .set_controller(ControllerType::Ps2)
);
}

Ps2Controller::~Ps2Controller()
{
    DeviceManager::unregister_device(devnode);
}
