// cpustat.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 31.03.26.
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

#include "cpustat.h"
#include <uapi/vespera/dev/cpustat.h>
#include <cpu/cpu_manager.h>

CpuStatDevice::CpuStatDevice()
    : CharDevice(BusType::VIRTUAL) {}

int CpuStatDevice::open(CharFile**) { return 0; }
int CpuStatDevice::release(CharFile*) { return 0; }

isize CpuStatDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer || count < sizeof(cpu_usage_info)) return -EINVAL;

    cpu_usage_info info{};
    info.cpu_count = cpu_manager::get_online_cpu_count();

    for (u32 i = 0; i < info.cpu_count && i < 64; i++) {
        const auto& ci = cpu_manager::cpu_infos[i];
        auto& stat = info.cpus[i];

        stat.cpu_id        = ci.cpu_id;
        stat.total_cycles  = ci.accounting.total_cycles;
        stat.idle_cycles   = ci.accounting.idle_cycles;
        stat.usage_percent = static_cast<u32>(
            cpu_manager::get_cpu_usage_percent(i)
        );
    }

    memcpy(buffer, &info, sizeof(cpu_usage_info));
    return sizeof(cpu_usage_info);
}

isize CpuStatDevice::write(CharFile*, const void*, const usize) {
    return -EUNSUPPORTED;
}