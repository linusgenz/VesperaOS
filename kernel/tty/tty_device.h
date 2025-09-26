// tty_device.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 25.09.25.
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

#ifndef VESPERAOS_TTY_DEVICE_H
#define VESPERAOS_TTY_DEVICE_H

#include <cstdint>
#include "../devices/chardevice.h"
#include "tty.h"

class TTYDevice : public CharDevice {
public:
    kernel::tty::TTY* tty;

    explicit TTYDevice(const char* name, kernel::tty::TTY* tty_ptr)
        : CharDevice(name, BusType::BUS_TTY), tty(tty_ptr) {}

    int open(CharFile** out_cf) override {
        *out_cf = new CharFile(this);
        return 0;
    }

    int release(CharFile* cf) override {
        delete cf;
        return 0;
    }

    size_t read(CharFile* cf, void* buffer, size_t count, size_t offset) override {
        return kernel::tty::tty_read(reinterpret_cast<char*>(buffer), count);
    }

    size_t write(CharFile* cf, const void* buffer, size_t count) override {
        const char* buf = static_cast<const char*>(buffer);
        for (size_t i = 0; i < count; i++) {
            kernel::tty::tty_process_output(tty, buf[i]);
        }
        return count;
    }
};


#endif //VESPERAOS_TTY_DEVICE_H