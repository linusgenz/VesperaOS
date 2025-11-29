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

#include "../chardevice.h"
#include "urandom.h"

URandomDevice::URandomDevice(const char* name, uint64_t seed)
    : CharDevice(name, BusType::VIRTUAL), state(seed) {}

int URandomDevice::open(CharFile** out_cf) {
    *out_cf = nullptr;
    return 0;
}

int URandomDevice::release(CharFile*) {
    return 0;
}

size_t URandomDevice::read(CharFile*, void* buffer, size_t count, size_t) {
    auto* out = static_cast<uint8_t*>(buffer);
    for (size_t i = 0; i < count; i++) {
        out[i] = next();
    }
    return count;
}

size_t URandomDevice::write(CharFile*, const void* buffer, size_t count) {
    (void)buffer;
    return count;
}

void URandomDevice::refill() {
    uint64_t x = state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    state = x;

    // in Bytes zerlegen (Little Endian)
    for (int i = 0; i < 8; i++) {
        buffer[i] = static_cast<uint8_t>(x >> (i * 8));
    }
    buffer_index = 0;
}

uint8_t URandomDevice::next() {
    if (buffer_index >= 8) {
        refill();
    }
    return buffer[buffer_index++];
}