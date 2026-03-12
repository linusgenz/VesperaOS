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
#ifndef VESPERAOS_USB_DEVICE_INFO_H
#define VESPERAOS_USB_DEVICE_INFO_H
#include "vespera/devices/char_device.h"
#include "vespera/devices/device_info.h"
#include "vespera/devices/usb_device_info.h"

class UsbDeviceInfo final : public IDeviceInfo, public IUsbDeviceInfo {
public:
    char model[128]{};
    char vendor[128]{};
    char serial[128]{};
    char firmware[16]{};

    usb_device_info_t usb_info{};

    bool get_model(char* out, usize len) override {
        strncpy(out, model, len);
        out[len - 1] = '\0';
        return model[0] != '\0';
    }
    bool get_serial(char* out, usize len) override {
        strncpy(out, serial, len);
        out[len - 1] = '\0';
        return serial[0] != '\0';
    }
    bool get_vendor(char* out, usize len) override {
        strncpy(out, vendor, len);
        out[len - 1] = '\0';
        return vendor[0] != '\0';
    }
    bool get_firmware(char* out, usize len) override {
        strncpy(out, firmware, len);
        out[len - 1] = '\0';
        return firmware[0] != '\0';
    }

    bool get_usb_device_info(usb_device_info_t* out) const override {
        if (!out) return false;
        *out = usb_info;
        return true;
    }
};

#endif  // VESPERAOS_USB_DEVICE_INFO_H
