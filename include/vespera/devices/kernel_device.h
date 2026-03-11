// kernel_device.h
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
#ifndef VESPERAOS_KERNEL_DEVICE_H
#define VESPERAOS_KERNEL_DEVICE_H

#include <klib/vector.h>
#include <vespera/types.h>

struct VfsNode;
class BlockDevice;
class CharDevice;
class IRenderDriver;
class ISmartDevice;
class IDriverLifecycle;
class IDeviceInfo;

enum class DeviceType : u8 {
    Other,
    Block,
    Char,
    Controller,
    Bus,
    Logical,
    Gpu,
};

enum class DeviceClass : u8 {
    Unknown,
    Storage,
    Usb,
    Input,
    Net,
    Misc,
    Pseudo,
    Graphics,
};

enum class ControllerType : u8 {
    None,
    Xhci,
    Ehci,
    Ohci,
    Uhci,
    Ahci,
    Nvme,
    VirtIo,
    Ps2,
    SmBus,
    IntelGpu,
    UefiGop,
    Other,
};

enum class BusType : u8 { VIRTUAL, None, Usb, Tty, I2C, Spi, Ps2, Pci };

struct KernelDevice {
    u32            id{};
    const char*    name{};
    DeviceType     type{DeviceType::Other};
    DeviceClass    dev_class{DeviceClass::Unknown};
    ControllerType controller{ControllerType::None};
    BusType        bus_type{BusType::None};

    VfsNode*              vfs_node_parent{};
    KernelDevice*         parent{nullptr};
    Vector<KernelDevice*> children;

    BlockDevice*      block{nullptr};
    CharDevice*       chardev{nullptr};
    IRenderDriver*    gpu{nullptr};

    ISmartDevice*     smart{nullptr};
    IDriverLifecycle* lifecycle{nullptr};
    IDeviceInfo*      info{nullptr};

    u32          next_nvme_index{0};
    Vector<bool> nvme_device_used;

    void* driver_data{nullptr};
};

#endif  // VESPERAOS_KERNEL_DEVICE_H
