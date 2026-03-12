// ioctl_usb_device.h
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
#ifndef VESPERAOS_IOCTL_USB_DEVICE_H
#define VESPERAOS_IOCTL_USB_DEVICE_H

#include <vespera/types.h>

#define USB_SPEED_FULL_SPEED 1
#define USB_SPEED_LOW_SPEED 2
#define USB_SPEED_HIGH_SPEED 3
#define USB_SPEED_SUPER_SPEED 4
#define USB_SPEED_SUPER_SPEED_PLUS 5

#define IOCTL_USB_GET_DEVICE_INFO 0x5500u

typedef struct usb_device_info {
    // Bus topology
    u8 bus_number;
    u8 slot_id;
    u8 port_num;
    u8 speed;  // USB_SPEED_* above

    u8 b_device_class;
    u8 b_device_subclass;
    u8 b_device_protocol;
    u8 _pad;

    u16 vendor_id;   // idVendor
    u16 product_id;  // idProduct
    u16 bcd_device;  // bcdDevice  (e.g. 0x0200 → "2.00")
    u16 bcd_usb;     // bcdUSB     (e.g. 0x0300 → USB 3.0)

    u8 num_configurations;
    u8 num_interfaces;
    u8 _pad2[2];
} usb_device_info_t;

#endif  // VESPERAOS_IOCTL_USB_DEVICE_H
