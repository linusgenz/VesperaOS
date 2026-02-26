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
#include <kernel/memory.h>

#include "../cpu/cpu_manager.h"
#include "../cpu/io.h"
#include "arch/x86_64/cpu/msr.h"

namespace kernel::memory
{
    void initialize_memory(BootInfo* bootInfo)
    {
        const uint64_t mMapEntries = bootInfo->mMapSize / bootInfo->mMapDescSize;

        initialize_page_frame_allocator(bootInfo->mMap, bootInfo->mMapSize, bootInfo->mMapDescSize);

        const auto kernelPhysStart = reinterpret_cast<uint64_t>(&_KernelStart) - bootInfo->kernel_virt_base;
        const auto kernelPhysEnd   = reinterpret_cast<uint64_t>(&_KernelEnd) - bootInfo->kernel_virt_base;
        const uint64_t kernelSize  = kernelPhysEnd - kernelPhysStart;
        const uint64_t kernelPages = kernelSize / 4096 + 1;

        lock_pages(&_KernelStart, kernelPages);
        lock_pages(nullptr, 256);

        initialize_page_table_manager();

        constexpr uint32_t IA32_PAT_MSR = 0x277;
        constexpr uint8_t PAT_UC = 0x00; // Uncacheable
        constexpr uint8_t PAT_WC = 0x01; // Write-Combining
        constexpr uint8_t PAT_WT = 0x04; // Write-Through
        constexpr uint8_t PAT_WP = 0x05; // Write-Protected
        constexpr uint8_t PAT_WB = 0x06; // Write-Back
        constexpr uint8_t PAT_UCM = 0x07; // Uncached (UC-)

        uint64_t pat_value =
            ((uint64_t)PAT_WB << 0) | // PAT0: Write-Back
            ((uint64_t)PAT_WC << 8) | // PAT1: Write-Combining
            ((uint64_t)PAT_UCM << 16) | // PAT2: UC-
            ((uint64_t)PAT_UC << 24) | // PAT3: UC
            ((uint64_t)PAT_WB << 32) | // PAT4: WB
            ((uint64_t)PAT_WT << 40) | // PAT5: WT
            ((uint64_t)PAT_UCM << 48) | // PAT6: UC-
            ((uint64_t)PAT_WP << 56); // PAT7: WP

        wrmsr(IA32_PAT_MSR, pat_value);
        outb(0x3F8, 'X');

        // just map everythin cuz it works lol. might not be a good practice tho, needs refactoring prob
        for (int i = 0; i < mMapEntries; i++)
        {
            auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<uint64_t>(bootInfo->mMap) + (i *
                bootInfo->mMapDescSize));
            //  if (desc->type != 7) continue; // Nur EfiConventionalMemory

            for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000)
            {
                map_memory(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(addr));
            }
        }
        outb(0x3F8, 'X');

       /*  const uint64_t kernelVirtStart = reinterpret_cast<uint64_t>(&_KernelStart);
        const uint64_t kernelVirtEnd   = reinterpret_cast<uint64_t>(&_KernelEnd);
       for (uint64_t virt = kernelVirtStart; virt < kernelVirtEnd; virt += 0x1000)
        {
            uint64_t phys = virt - bootInfo->kernel_virt_base;
            map_memory(reinterpret_cast<void*>(virt),
                       reinterpret_cast<void*>(phys));
        }*/
        outb(0x3F8, 'X');

        for (uint32_t i = 0; i < CPUManager::total_cpus; ++i)
        {
            auto stack_addr = reinterpret_cast<void*>(static_cast<uintptr_t>(KERNEL_STACK_BASE) + i * static_cast<
                uintptr_t>(KERNEL_STACK_SIZE));
            map_memory(stack_addr, stack_addr,
                       (1ULL << WriteThrough) | (1ULL << CacheDisabled));
        }
        outb(0x3F8, 'X');

        map_memory(reinterpret_cast<void*>(0x1000), reinterpret_cast<void*>(0x1000),
                   (1ULL << WriteThrough) | (1ULL << CacheDisabled));
        map_memory(reinterpret_cast<void*>(0x2000), reinterpret_cast<void*>(0x2000), (1ULL << CacheDisabled));
        outb(0x3F8, 'X');

        // Framebuffer: virt (HHDM) → phys
        auto fb_virt = reinterpret_cast<uint64_t>(bootInfo->framebuffer->base_address);
        auto fb_phys = bootInfo->framebuffer->phys_base_address;
        uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;

        lock_pages(reinterpret_cast<void*>(fb_phys), fb_size / 0x1000 + 1);
        for (uint64_t offset = 0; offset < fb_size; offset += 0x1000) {
            map_memory(
                reinterpret_cast<void*>(fb_virt + offset),
                reinterpret_cast<void*>(fb_phys + offset),
                (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::Global)
            );
        }
        outb(0x3F8, 'X');


        outb(0x3F8, 'D');
        asm ("mov %0, %%cr3" : : "r" (get_pagetable_address()));
        outb(0x3F8, 'P');
    }
}
