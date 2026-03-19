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

#include <klib/string.h>
#include <vespera/mm/memory.h>

#include "../../kversion.h"

VersionDevice::VersionDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int VersionDevice::open(CharFile**) {
    return 0;
}

int VersionDevice::release(CharFile*) {
    return 0;
}

isize VersionDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer || count == 0) return -EINVAL;

    const char* ver = get_kernel_version();
    usize len = strlen(ver);
    if (count < len) len = count;

    memcpy(buffer, ver, len);
    return static_cast<isize>(len);
}

isize VersionDevice::write(CharFile*, const void*, usize) {
    return -EUNSUPPORTED;  // why would you write on the version device lol
}
