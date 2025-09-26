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

#include "../../filesystem/vfs/vfs_node.h"
#include "../../filesystem/devfs/devfs.h"
#include <cstddef>
#include <cstdint>
#include <errno.h>

struct CharFile;

// Flags for poll()
enum PollMask : int {
    POLLIN  = 0x01,
    POLLOUT = 0x02
};

class CharDevice {
public:
    const char* name;
    BusType bus_type;

    explicit CharDevice(const char* name, BusType bus_type) : name(name), bus_type(bus_type) {
        DevFS::register_device(this);
    }

    virtual ~CharDevice() {
        if (name) {
            kernel::memory::free((void*)name);
            name = nullptr;
        }
    }

    virtual int open(CharFile** out_cf) = 0;
    virtual int release(CharFile* cf) = 0;

    virtual size_t read(CharFile* cf, void* buffer, size_t count, size_t offset) = 0;
    virtual size_t write(CharFile* cf, const void* buffer, size_t count) = 0;

    virtual int ioctl(CharFile* cf, uint32_t cmd, void* arg) { return -ENOTTY; }

    virtual int poll(CharFile* cf) { return 0; }

    int register_device() {
        return DevFS::register_device(this);
    }

    [[nodiscard]] int unregister_device() const {
        return DevFS::unregister_device(name);
    };

    // Nicht-kopierbar
    CharDevice(const CharDevice&) = delete;
    CharDevice& operator=(const CharDevice&) = delete;
};


#endif //VESPERAOS_CHAR_DEVICE_BASE_H