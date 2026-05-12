// cpuinfo.cpp
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

#include "cpuinfo.h"

#include <kernel/cpu/cpu.h>
#include <klib/string.h>
#include <vespera/mm/memory.h>

CpuInfoDevice::CpuInfoDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int CpuInfoDevice::open(CharFile**) {
    return 0;
}

int CpuInfoDevice::release(CharFile*) {
    return 0;
}

isize CpuInfoDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer || count < sizeof(CpuInfo)) return -EINVAL;

    CpuInfo info{};
    get_cpu_vendor(info.vendor);
    get_cpu_brand(info.brand);
    info.features = check_cpu_features();

    // trim white space
    usize len = strlen(info.brand);
    while (len > 0 && info.brand[len - 1] == ' ') {
        info.brand[len - 1] = '\0';
        len--;
    }

    memcpy(buffer, &info, sizeof(CpuInfo));
    return sizeof(CpuInfo);
}

isize CpuInfoDevice::write(CharFile*, const void*, const usize /*count*/) {
    return -EUNSUPPORTED;
}