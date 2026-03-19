// zero.cpp
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

#include "zero.h"

#include <vespera/devices/char_device.h>
#include <vespera/mm/memory.h>

ZeroDevice::ZeroDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int ZeroDevice::open(CharFile**) {
    return 0;
}

int ZeroDevice::release(CharFile*) {
    return 0;
}

isize ZeroDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer) return -EINVAL;
    memset(buffer, 0, count);
    return count;
}

isize ZeroDevice::write(CharFile*, const void*, const usize count) {
    return count;
}
