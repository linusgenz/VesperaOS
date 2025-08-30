// xhci_usb_device_driver.h
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

#ifndef VESPERAOS_XHCI_USB_DEVICE_DRIVER_H
#define VESPERAOS_XHCI_USB_DEVICE_DRIVER_H

namespace USB {
    class xhciDriver;
}

class xhciUsbInterface;
class xhciDevice;

class xhciUsbDeviceDriver {
public:
    xhciUsbDeviceDriver() = default;
    ~xhciUsbDeviceDriver() = default;

    void attach_interface(xhciUsbInterface* interface);

    virtual void on_startup(USB::xhciDriver* hcd, xhciDevice* dev) = 0;
    virtual void on_event(USB::xhciDriver* hcd, xhciDevice* dev) = 0;

protected:
    xhciUsbInterface* m_interface;
};


#endif //VESPERAOS_XHCI_USB_DEVICE_DRIVER_H