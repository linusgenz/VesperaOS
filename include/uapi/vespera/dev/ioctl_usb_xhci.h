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

#ifndef VESPERAOS_IOCTL_USB_XHCI_H
#define VESPERAOS_IOCTL_USB_XHCI_H

#include <vespera/types.h>

/**
 * @brief Represents the status and identifying information of a single xHCI USB device.
 */
typedef struct xhci_device_stat {
    u8 slot_id;           ///< USB slot ID assigned by the xHCI controller
    u8 port_num;          ///< Physical port number the device is connected to
    u8 speed;             ///< Device speed (e.g., 0=Low, 1=Full, 2=High, 3=Super)
    u8 bus_number;        ///< Bus number of the device
    u16 vendor_id;        ///< USB Vendor ID
    u16 product_id;       ///< USB Product ID
    char product[64];          ///< Product string (null-terminated)
    char manufacturer[64];     ///< Manufacturer string (null-terminated)
    char serial_number[64];    ///< Serial number string (null-terminated)
} xhci_device_stat_t;

/**
 * @brief IOCTL code to get the number of devices currently connected to the xHCI controller.
 */
#define XHCI_IOCTL_GET_COUNT   1

/**
 * @brief IOCTL code to get information about a single device by index.
 *
 * Use this in combination with xhci_device_stat struct to retrieve device info.
 */
#define XHCI_IOCTL_GET_DEVICE  2

#endif //VESPERAOS_IOCTL_USB_XHCI_H