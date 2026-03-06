// usb_xhci_ioctl.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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

#ifndef VESPERAOS_USB_XHCI_IOCTL_H
#define VESPERAOS_USB_XHCI_IOCTL_H

#include <stdint.h>

struct XhciDeviceStat {
    uint8_t slot_id;
    uint8_t port_num;
    uint8_t speed;
    uint8_t bus_number;
    uint16_t vendor_id;
    uint16_t product_id;
    char product[64];
    char manufacturer[64];
    char serial_number[64];
};

#define XHCI_IOCTL_GET_COUNT 1
#define XHCI_IOCTL_GET_DEVICE 2

#endif  // VESPERAOS_USB_XHCI_IOCTL_H