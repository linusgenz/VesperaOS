// fs_registry.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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

#include "fs_registry.h"

#include <klib/string.h>

static FileSystemDriver* fs_drivers[MAX_FS_DRIVERS];
static usize driver_count = 0;

void register_fs_driver(FileSystemDriver* driver) {
    if (driver_count >= MAX_FS_DRIVERS) return;

    fs_drivers[driver_count++] = driver;
}

FileSystemDriver* find_fs_driver(const char* name) {
    for (usize i = 0; i < driver_count; ++i) {
        if (strcmp(fs_drivers[i]->name, name) == 0) {
            return fs_drivers[i];
        }
    }
    return nullptr;
}

usize fs_driver_count() {
    return driver_count;
}

FileSystemDriver* fs_driver_at(usize i) {
    if (i >= driver_count) return nullptr;
    return fs_drivers[i];
}
