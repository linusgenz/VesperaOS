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

    // Kernel-Pages sperren damit der Allocator sie nicht hergibt
    const auto kernelPhysStart = reinterpret_cast<uint64_t>(&_KernelStart) - bootInfo->kernel_virt_base + bootInfo->kernel_phys_base;
    const auto kernelPhysEnd   = reinterpret_cast<uint64_t>(&_KernelEnd)   - bootInfo->kernel_virt_base + bootInfo->kernel_phys_base;
    const uint64_t kernelPages = (kernelPhysEnd - kernelPhysStart) / 4096 + 1;
    lock_pages(reinterpret_cast<void*>(kernelPhysStart), kernelPages);
    lock_pages(nullptr, 256); // Low Memory reservieren

    // Page Tables aufsetzen (HHDM + Kernel)
    initialize_page_table_manager(bootInfo);

    // PAT konfigurieren
    constexpr uint32_t IA32_PAT_MSR = 0x277;
    uint64_t pat_value =
        (0x06ULL <<  0) | // PAT0: WB
        (0x01ULL <<  8) | // PAT1: WC
        (0x07ULL << 16) | // PAT2: UC-
        (0x00ULL << 24) | // PAT3: UC
        (0x06ULL << 32) | // PAT4: WB
        (0x04ULL << 40) | // PAT5: WT
        (0x07ULL << 48) | // PAT6: UC-
        (0x05ULL << 56);  // PAT7: WP
    wrmsr(IA32_PAT_MSR, pat_value);

    // Framebuffer mappen (HHDM-virt → phys, mit WC/WT für Performance)
    const uint64_t fb_virt = reinterpret_cast<uint64_t>(bootInfo->framebuffer->base_address);
    const uint64_t fb_phys = bootInfo->framebuffer->phys_base_address;
    const uint64_t fb_size = bootInfo->framebuffer->buffer_size + 0x1000;
    lock_pages(reinterpret_cast<void*>(fb_phys), fb_size / 0x1000 + 1);
    for (uint64_t offset = 0; offset < fb_size; offset += 0x1000) {
        map_memory(
            reinterpret_cast<void*>(fb_virt + offset),
            reinterpret_cast<void*>(fb_phys + offset),
            (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::Global)
        );
    }

    asm volatile("mov %0, %%cr3" : : "r"(get_pagetable_address()) : "memory");
    g_hhdm_offset       = bootInfo->hhdm_offset;
    relocate_bitmap_to_hhdm();
    outb(0x3F8, 'P');
}
}
