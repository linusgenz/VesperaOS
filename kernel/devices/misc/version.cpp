// version.cpp
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

#include "version.h"
#include <string.h>

#include "../../kversion.h"

VersionDevice::VersionDevice(const char* name)
    : CharDevice(name, BusType::VIRTUAL) {}

int VersionDevice::open(CharFile** out_cf) {
    *out_cf = nullptr;
    return 0;
}

int VersionDevice::release(CharFile*) {
    return 0;
}

size_t VersionDevice::read(CharFile*, void* buffer, size_t count, size_t) {
    const char* ver = get_kernel_version();
    size_t len = strlen(ver);
    if (count < len) len = count;
    memcpy(buffer, ver, len);
    return len;
}

size_t VersionDevice::write(CharFile*, const void*, size_t count) {
    return count;
}