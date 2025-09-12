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

void ProcessMemoryManager::track_user_page(void *phys_addr, void *virt_addr) {
    if (!process) return;

    auto *page = static_cast<user_page *>(kernel::memory::malloc(sizeof(user_page)));
    if (!page) return;

    page->phys_addr = phys_addr;
    page->virt_addr = virt_addr;
    page->next = user_pages_head;
    user_pages_head = page;
}

bool ProcessMemoryManager::map_and_track_memory(void *virtual_addr, void *physical_addr, const uint64_t flags) {
    if (!virtual_addr || !physical_addr || !process) return false;
    kernel::memory::map_memory(virtual_addr, physical_addr, flags);

    track_user_page(physical_addr, virtual_addr);

    return true;
}

bool ProcessMemoryManager::map_and_track_range(void *virtual_addr,
                                               void *physical_addr,
                                               const size_t size,
                                               const uint64_t flags) {
    if (!virtual_addr || !physical_addr || !process) return false;

    kernel::memory::map_range(virtual_addr, physical_addr, size,
                              flags, process);

    auto ps = reinterpret_cast<uintptr_t>(physical_addr);
    auto vs = reinterpret_cast<uintptr_t>(virtual_addr);
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        track_user_page(reinterpret_cast<void *>(ps + offset), reinterpret_cast<void *>(vs + offset));
    }

    return true;
}

void ProcessMemoryManager::cleanup_process_pages() {
    if (!process) return;

    auto *current = user_pages_head;
    while (current) {
        auto *next = current->next;

        if (current->virt_addr) {
            kernel::memory::unmap_memory(current->virt_addr);
        }

        if (current->phys_addr) {
            kernel::memory::free_page(current->phys_addr);
        }

        kernel::memory::free(current);

        current = next;
    }

    user_pages_head = nullptr;
}
