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

#include <kernel/memory.h>
#include <string.h>

#include "../../cpu/cpu.h"

CpuInfoDevice::CpuInfoDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int CpuInfoDevice::open(CharFile**) {
    return 0;
}

int CpuInfoDevice::release(CharFile*) {
    return 0;
}

ssize_t CpuInfoDevice::read(CharFile*, void* buffer, const size_t count, size_t) {
    if (!buffer || count < sizeof(CpuInfo)) return -EINVAL;

    CpuInfo info{};
    get_cpu_vendor(info.vendor);
    get_cpu_brand(info.brand);
    info.features = check_cpu_features();

    // trim white space
    size_t len = strlen(info.brand);
    while (len > 0 && info.brand[len - 1] == ' ') {
        info.brand[len - 1] = '\0';
        len--;
    }

    memcpy(buffer, &info, sizeof(CpuInfo));
    return sizeof(CpuInfo);
}

ssize_t CpuInfoDevice::write(CharFile*, const void*, const size_t /*count*/) {
    return -EUNSUPPORTED;
}