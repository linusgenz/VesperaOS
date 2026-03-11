// device_descriptor.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.03.26.
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
#ifndef VESPERAOS_DEVICE_DESCRIPTOR_H
#define VESPERAOS_DEVICE_DESCRIPTOR_H

#include "kernel_device.h"

struct DeviceDescriptor {
    const char*    name       = nullptr;
    DeviceType     type       = DeviceType::Other;
    DeviceClass    dev_class  = DeviceClass::Unknown;
    ControllerType controller = ControllerType::None;
    BusType        bus_type   = BusType::None;
    KernelDevice*  parent     = nullptr;

    BlockDevice*      block     = nullptr;
    CharDevice*       chardev   = nullptr;
    IRenderDriver*    gpu       = nullptr;
    ISmartDevice*     smart     = nullptr;
    IDriverLifecycle* lifecycle = nullptr;
    IDeviceInfo*      info      = nullptr;

    DeviceDescriptor& set_name(const char* n)          { name       = n; return *this; }
    DeviceDescriptor& set_type(DeviceType t)            { type       = t; return *this; }
    DeviceDescriptor& set_class(DeviceClass c)          { dev_class  = c; return *this; }
    DeviceDescriptor& set_controller(ControllerType ct) { controller = ct; return *this; }
    DeviceDescriptor& set_bus(BusType b)                { bus_type   = b; return *this; }
    DeviceDescriptor& with_parent(KernelDevice* p)      { parent     = p; return *this; }

    DeviceDescriptor& with_block(BlockDevice* b)          { block     = b; return *this; }
    DeviceDescriptor& with_char(CharDevice* c)            { chardev   = c; return *this; }
    DeviceDescriptor& with_gpu(IRenderDriver* g)          { gpu       = g; return *this; }
    DeviceDescriptor& with_smart(ISmartDevice* s)         { smart     = s; return *this; }
    DeviceDescriptor& with_lifecycle(IDriverLifecycle* l) { lifecycle = l; return *this; }
    DeviceDescriptor& with_info(IDeviceInfo* i)           { info      = i; return *this; }
};


#endif  // VESPERAOS_DEVICE_DESCRIPTOR_H
