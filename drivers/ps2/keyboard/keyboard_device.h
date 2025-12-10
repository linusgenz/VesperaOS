/**
 * @file keyboard_device.h
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
#ifndef VESPERAOS_KEYBOARD_DEVICE_H
#define VESPERAOS_KEYBOARD_DEVICE_H

#include <kernel/devices/char_device.h>
#include <kernel/devices/device_manager.h>

class Ps2Controller;

class Ps2KeyboardDevice final : public CharDevice {
public:
    KernelDevice* devnode;
    Ps2Controller* parent;

    explicit Ps2KeyboardDevice(Ps2Controller* controller);
    ~Ps2KeyboardDevice() override;

    int open(CharFile** out_cf) override;
    int release(CharFile* cf) override;
    ssize_t read(CharFile* cf, void* buffer, size_t count, size_t offset) override;
    ssize_t write(CharFile* cf, const void* buffer, size_t count) override;
};

#endif //VESPERAOS_KEYBOARD_DEVICE_H