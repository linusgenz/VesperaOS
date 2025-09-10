// process_memory_manager.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 09.09.25.
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

#include "process_memory_manager.h"
#include <cstdint>
#include <memory.h>
#include "process.h"

void ProcessMemoryManager::track_user_page(void* phys_addr) {
    if (!process) return;

    user_page* page = (user_page*)kernel::memory::request_page();
    if (!page) return;

    page->phys_addr = phys_addr;
    page->next = process->user_pages_head;
    process->user_pages_head = page;
}

bool ProcessMemoryManager::map_and_track_memory(void* virtual_addr,
                                                void* physical_addr,
                                                size_t size,
                                                uint64_t flags) {
    // Erst mappen (ohne Process-Tracking)
    kernel::memory::map_range(virtual_addr, physical_addr, size,
                              flags & ~(1ULL << PT_Flag::UserSuper), process);

    // Dann User-Flags setzen falls nötig
    if (flags & (1ULL << PT_Flag::UserSuper)) {
        kernel::memory::set_user_flags(virtual_addr, size);

        // Pages für den Process tracken
        uintptr_t start = (uintptr_t)physical_addr;
        uintptr_t end = start + size;

        for (uintptr_t addr = start; addr < end; addr += 0x1000) {
            track_user_page((void*)addr);
        }
    }

    return true;
}

bool ProcessMemoryManager::map_and_track_range(void* virtual_addr,
                                               void* physical_addr,
                                               size_t size,
                                               uint64_t flags) {
    return map_and_track_memory(virtual_addr, physical_addr, size, flags);
}