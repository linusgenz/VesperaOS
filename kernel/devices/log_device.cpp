// log_device.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 01.10.25.
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

#include "log_device.h"

#include <errno.h>

int LogDevice::open(CharFile** out_cf) {
    if (!out_cf) return -EINVAL;

    auto* cf = new CharFile();
    cf->driver_private = global_channel;
    *out_cf = cf;
    return 0;
}

int LogDevice::release(CharFile* cf) {
    delete cf;
    return 0;
}

ssize_t LogDevice::write(CharFile* cf, const void* buffer, size_t count) {
    if (!cf || !buffer || count == 0) return -EINVAL;
    auto* channel = static_cast<Channel*>(cf->driver_private);

    ssize_t w;
    while ((w = channel->send(buffer, count)) == -EAGAIN) {
        asm volatile("pause");
    }

    return w;
}

ssize_t LogDevice::read(CharFile* cf, void* buffer, size_t count, size_t offset) {
    if (!cf || !buffer || count == 0) return -EINVAL;
    auto* channel = static_cast<Channel*>(cf->driver_private);

    ssize_t r;
    while ((r = channel->recv(buffer, count)) == -EAGAIN) {
        asm volatile("pause");
    }

    return r;
}

int LogDevice::poll(CharFile* cf) {
    if (!cf) return 0;
    auto* channel = static_cast<Channel*>(cf->driver_private);
    int mask = 0;
    if (channel->used > 0) mask |= POLLIN;
    if (channel->used < channel->capacity) mask |= POLLOUT;
    return mask;
}
