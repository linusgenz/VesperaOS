// xhci_usb_interface.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 28.08.25.
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

#ifndef VESPERAOS_XHCU_USB_INTERFACE_H
#define VESPERAOS_XHCU_USB_INTERFACE_H

#include <klib/vector.h>

#include "../usb_descriptors.h"
#include "xhci_endpoint.h"
#include "xhci_usb_device_driver.h"

class XhciUsbInterface {
   public:
    XhciUsbInterface(u8 dev_slot_id, const USB_INTERFACE_DESCRIPTOR *desc);

    ~XhciUsbInterface() = default;

    void setup_add_endpoint(const USB_ENDPOINT_DESCRIPTOR *ep_desc);

    USB_INTERFACE_DESCRIPTOR descriptor{};
    Vector<XhciEndpoint *> endpoints;
    XhciUsbDeviceDriver *driver{};

    // HID report data for HID devices
    u8 *additional_data = nullptr;
    usize additional_data_length = 0;

   private:
    u8 dev_slot_id_;
};

#endif  // VESPERAOS_XHCU_USB_INTERFACE_H
