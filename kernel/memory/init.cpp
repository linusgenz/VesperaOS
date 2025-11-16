// init.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 15.11.25.
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

#include <boot.h>
#include <memory.h>
#include "stack_manager.h"
#include "../cpu/cpu_manager.h"

namespace kernel::memory {
    void initialize_memory(BootInfo *bootInfo) {
        const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

        initialize_page_frame_allocator(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

        const uint64_t kernelStart = reinterpret_cast<uint64_t>(&_KernelStart);
        const uint64_t kernelEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
        const uint64_t kernelSize = kernelEnd - kernelStart;
        const uint64_t kernelPages = kernelSize / 4096 + 1;

        lock_pages(&_KernelStart, kernelPages);
        lock_pages(nullptr, 256);

        initialize_page_table_manager();

        // just map everythin cuz it works lol. might not be a good practice tho, needs refactoring prob
        for (int i = 0; i < mMapEntries; i++) {
            EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR *) (
                (uint64_t) bootInfo->mMap + (i * bootInfo->mMapDescSize));
            //  if (desc->type != 7) continue; // Nur EfiConventionalMemory

            for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000) {
                map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
            }
        }

        for (uint64_t addr = kernelStart; addr < kernelEnd; addr += 0x1000) {
            map_memory(reinterpret_cast<void *>(addr), reinterpret_cast<void *>(addr));
        }

        for (uint32_t i = 0; i < CPUManager::total_cpus; ++i) {
            void *stack_addr = (void *) (KERNEL_STACK_BASE + i * KERNEL_STACK_SIZE);
            map_memory(stack_addr, stack_addr,
                       (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        }

        map_memory((void *) 0x8000, (void *) 0x8000,
                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        map_memory((void *) 0x7000, (void *) 0x7000,
                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        map_memory((void *) 0x6000, (void *) 0x6000,
                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        map_memory((void *) 0x1000, (void *) 0x1000,
                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));
        map_memory((void *) 0x2000, (void *) 0x2000, (1ULL << PT_Flag::CacheDisabled));

        uint64_t fb_base = (uint64_t) bootInfo->framebuffer->base_address;
        uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
        lock_pages((void *) fb_base, fb_size / 0x1000 + 1);
        for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000) {
            map_memory((void *) t, (void *) t);
        }

        asm ("mov %0, %%cr3" : : "r" (get_pagetable_address()));
    }
}
