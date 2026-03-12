// usb_device_info.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.03.26.
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
#ifndef VESPERAOS_I_USB_DEVICE_INFO_H
#define VESPERAOS_I_USB_DEVICE_INFO_H

#include <uapi/vespera/dev/ioctl_usb_device.h>
#include <vespera/devices/device_info.h>

class IUsbDeviceInfo {
public:
    virtual bool get_usb_device_info(usb_device_info_t* out) const = 0;
    virtual ~IUsbDeviceInfo() = default;
};


#endif  // VESPERAOS_I_USB_DEVICE_INFO_H
