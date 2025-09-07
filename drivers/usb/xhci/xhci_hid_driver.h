// xhci_hid_driver.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.09.25.
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

#ifndef VESPERAOS_XHCI_HID_DRIVER_H
#define VESPERAOS_XHCI_HID_DRIVER_H

#include "xhci_usb_device_driver.h"
#include <cstdint>

class xhciHidDriver : public xhciUsbDeviceDriver {
public:
    xhciHidDriver() = default;
    ~xhciHidDriver() override = default;

    virtual void on_device_init() = 0;
    virtual void on_device_event(uint8_t* data) = 0;

    void on_startup(USB::xhciDriver* hcd, xhciDevice* dev) override;
    void on_event(USB::xhciDriver* hcd, xhciDevice* dev) override;

private:
    void request_hid_report(USB::xhciDriver* hcd, xhciDevice* dev);
};

#endif //VESPERAOS_XHCI_HID_DRIVER_H