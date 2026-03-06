// urandom.cpp
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

#include "urandom.h"

#include "../../../include/kernel/devices/char_device.h"

URandomDevice::URandomDevice(const uint64_t seed)
    : CharDevice( BusType::VIRTUAL)
    , state_(seed) {
}

int URandomDevice::open(CharFile**) {
    return 0;
}

int URandomDevice::release(CharFile*) {
    return 0;
}

ssize_t URandomDevice::read(CharFile*, void* buffer, const size_t count, size_t) {
    if (count < sizeof(uint8_t) || !buffer) return -EINVAL;

    auto* out = static_cast<uint8_t*>(buffer);
    for (size_t i = 0; i < count; i++) {
        out[i] = next();
    }
    return static_cast<ssize_t>(count);
}

ssize_t URandomDevice::write(CharFile*, const void* buffer, const size_t count) {
    (void)buffer;
    return -EUNSUPPORTED;
}

void URandomDevice::refill() {
    uint64_t x = state_;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state_ = x;

    // in Bytes zerlegen (Little Endian)
    for (int i = 0; i < 8; i++) {
        dev_buffer_[i] = static_cast<uint8_t>(x >> (i * 8));
    }
    buffer_index_ = 0;
}

uint8_t URandomDevice::next() {
    if (buffer_index_ >= 8) {
        refill();
    }
    return dev_buffer_[buffer_index_++];
}
