// rtc.cpp
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

#include "rtc.h"

#include <vespera/mm/memory.h>
#include <vespera/time.h>

RtcDevice::RtcDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int RtcDevice::open(CharFile**) {
    return 0;
}

int RtcDevice::release(CharFile*) {
    return 0;
}

isize RtcDevice::read(CharFile*, void* buffer, usize count, usize) {
    if (count < sizeof(RtcData) || !buffer) return -EINVAL;

    RtcData data{};
    kernel::time::read_rtc(data.sec, data.min, data.hour, data.day, data.month, data.year);

    memcpy(buffer, &data, sizeof(RtcData));
    return sizeof(RtcData);
}

isize RtcDevice::write(CharFile*, const void* buffer, usize count) {
    (void)buffer;
    return -EUNSUPPORTED;  // maybe add cmos write support late
}
