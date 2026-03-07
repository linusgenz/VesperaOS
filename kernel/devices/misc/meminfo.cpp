// meminfo.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 19.11.25.
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

#include "meminfo.h"

#include <vespera/mm/memory.h>
#include <vespera/system/system_manager.h>

MemInfoDevice::MemInfoDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int MemInfoDevice::open(CharFile**) {
    return 0;
}

int MemInfoDevice::release(CharFile*) {
    return 0;
}

isize MemInfoDevice::read(CharFile*, void* buffer, usize count, usize) {
    if (count < sizeof(meminfo_t) || !buffer) return -EINVAL;

    kernel::SystemManager::update_system_stats();
    const kernel::SystemStats stats = kernel::SystemManager::get_system_stats();

    meminfo_t info{};
    info.total_ram = stats.total_memory;
    info.used_ram = stats.used_memory;
    info.free_ram = stats.free_memory;
    info.reserved_ram = stats.reserved_memory;

    memcpy(buffer, &info, sizeof(meminfo_t));
    return sizeof(meminfo_t);
}

isize MemInfoDevice::write(CharFile*, const void* buffer, usize count) {
    (void)buffer;
    return -EUNSUPPORTED;
}
