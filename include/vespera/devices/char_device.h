// chardevice.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 12.09.25.
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

#ifndef VESPERAOS_CHAR_DEVICE_BASE_H
#define VESPERAOS_CHAR_DEVICE_BASE_H

#include <vespera_errno.h>

#include "../../klib/string.h"
#include "device_manager.h"

struct CharFile {
    void* driver_private;
};

class CharDevice {
   public:
    BusType bus_type;

    explicit CharDevice(BusType bus_type)
        : bus_type(bus_type) {
    }
    virtual ~CharDevice() = default;

    virtual int open(CharFile** out_cf) = 0;
    virtual int release(CharFile* cf) = 0;

    virtual isize read(CharFile* cf, void* buffer, usize count, usize offset) = 0;
    virtual isize write(CharFile* cf, const void* buffer, usize count) = 0;

    virtual int ioctl(CharFile*, u32, void*) {
        return -ENOTTY;
    }

    virtual int poll(CharFile*) {
        return 0;
    }

    // Non-copyable
    CharDevice(const CharDevice&) = delete;
    CharDevice& operator=(const CharDevice&) = delete;
};

#endif  // VESPERAOS_CHAR_DEVICE_BASE_H
