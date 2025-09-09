// process_memory_manager.h
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

#ifndef VESPERAOS_PROCESS_MEMORY_MANAGER_H
#define VESPERAOS_PROCESS_MEMORY_MANAGER_H
#include "process.h"

class ProcessMemoryManager {
private:
    kprocess_t* process;

public:
    explicit ProcessMemoryManager(kprocess_t* proc) : process(proc) {}

    // Tracks a physical page as belonging to this process
    void track_user_page(void* phys_addr);

    // Maps memory and tracks it for the process
    bool map_and_track_memory(void* virtual_addr, void* physical_addr,
                              size_t size, uint64_t flags);

    // Maps a range and tracks all pages
    bool map_and_track_range(void* virtual_addr, void* physical_addr,
                             size_t size, uint64_t flags);
};

#endif //VESPERAOS_PROCESS_MEMORY_MANAGER_H