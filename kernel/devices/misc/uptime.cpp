// uptime.cpp
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

#include "uptime.h"

#include <kernel/memory.h>
#include <kernel/time.h>

UptimeDevice::UptimeDevice()
    : CharDevice( BusType::VIRTUAL) {
}

int UptimeDevice::open(CharFile**) {
    return 0;
}

int UptimeDevice::release(CharFile*) {
    return 0;
}

ssize_t UptimeDevice::read(CharFile*, void* buffer, size_t count, size_t) {
    if (count < sizeof(uint64_t) || !buffer) return -EINVAL;

    uint64_t uptime = kernel::time::get_uptime_ms();
    memcpy(buffer, &uptime, sizeof(uint64_t));
    return sizeof(uint64_t);
}

ssize_t UptimeDevice::write(CharFile*, const void* buffer, size_t count) {
    (void)buffer;
    return -EUNSUPPORTED;
}
