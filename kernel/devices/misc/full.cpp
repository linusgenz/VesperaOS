// full.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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

#include <errno.h>
#include "full.h"

FullDevice::FullDevice(const char *name)
    : CharDevice(name, BusType::VIRTUAL) {
}

int FullDevice::open(CharFile **out_cf) {
    *out_cf = nullptr;
    return 0;
}

int FullDevice::release(CharFile *cf) {
    (void) cf;
    return 0;
}

size_t FullDevice::read(CharFile *cf, void *buffer, size_t count, size_t offset) {
    (void) cf;
    (void) offset;
    memset(buffer, 0, count);
    return count;
}

size_t FullDevice::write(CharFile *cf, const void *buffer, size_t count) {
    (void) cf;
    (void) buffer;
    (void) count;
    // always fail with ENOSPC
    return -ENOSPC;
}
