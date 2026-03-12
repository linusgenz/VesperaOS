/**
 * @file xhci_keyboard_device.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 10.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_XHCI_KEYBOARD_DEVICE_H
#define VESPERAOS_XHCI_KEYBOARD_DEVICE_H

#include <vespera/devices/char_device.h>
#include <vespera/devices/device_manager.h>
#include <vespera/types.h>

class UsbDeviceInfo;
class UsbKeyboardDevice final : public CharDevice
{
public:
    explicit UsbKeyboardDevice(const char* name, KernelDevice* parent, UsbDeviceInfo* info = nullptr);
    int open(CharFile**) override;
    int release(CharFile*) override;

    ~UsbKeyboardDevice() override;

    KernelDevice* kd;

    // CharDevice API
    isize read(CharFile* cf, void* buffer, usize count, usize offset) override;
    isize write(CharFile* cf, const void* buffer, usize count) override;
};

#endif //VESPERAOS_XHCI_KEYBOARD_DEVICE_H
