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
#include "uapi/vespera/dev/cpuinfo.h"

#include <cpu/cpu.h>
#include <klib/string.h>

#include "cpu/cpu_manager.h"

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
    if (!buffer || count < sizeof(cpu_info)) return -EINVAL;

    cpu_info info{};
    get_cpu_vendor(info.vendor);
    get_cpu_brand(info.brand);
    info.features = check_cpu_features();
    info.cores = cpu_manager::get_online_cpu_count();
    info.total_cores = cpu_manager::get_total_cpu_count();

    // trim white space
    usize len = strlen(info.brand);
    while (len > 0 && info.brand[len - 1] == ' ') {
        info.brand[len - 1] = '\0';
        len--;
    }

    memcpy(buffer, &info, sizeof(cpu_info));
    return sizeof(cpu_info);
}

isize CpuInfoDevice::write(CharFile*, const void*, const usize /*count*/) {
    return -EUNSUPPORTED;
}