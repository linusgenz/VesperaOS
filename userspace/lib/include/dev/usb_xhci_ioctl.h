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

/**
 * @brief Represents the status and identifying information of a single xHCI USB device.
 */
typedef struct {
    uint8_t slot_id;           ///< USB slot ID assigned by the xHCI controller
    uint8_t port_num;          ///< Physical port number the device is connected to
    uint8_t speed;             ///< Device speed (e.g., 0=Low, 1=Full, 2=High, 3=Super)
    uint8_t bus_number;        ///< Bus number of the device
    uint16_t vendor_id;        ///< USB Vendor ID
    uint16_t product_id;       ///< USB Product ID
    char product[64];          ///< Product string (null-terminated)
    char manufacturer[64];     ///< Manufacturer string (null-terminated)
    char serial_number[64];    ///< Serial number string (null-terminated)
} xhci_device_stat;

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

#endif //VESPERAOS_USB_XHCI_IOCTL_H