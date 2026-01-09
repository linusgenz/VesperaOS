/**
 * @file bootstrap.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/

#include "panic/panic.h"
#include <bootstrap.h>
#include <paging/page_table_manager.h>

#include "../kernel/paging/page_table_manager.h"
#include "paging/page_frame_allocator.h"

extern "C" void enter_higher_half(
    uint64_t pml4,
    uint64_t stack_top,
    KernelEntry kernel_entry,
    void* boot_info
);

constexpr uint64_t KERNEL_VIRT_BASE = 0xFFFFFFFF80000000ULL;
constexpr uint64_t KERNEL_STACK_PAGES = 8;

PageFrameAllocator allocator = PageFrameAllocator();
PageTableManager ptm = PageTableManager(nullptr);

extern "C" void bootstrap_main(BootstrapInfo* info)
{
    if (!info)
        return;
    if (!info->bi.mmap)
        panic("Invalid BootInfo", info->bi.fb);

    memset(info->bi.fb->base_address, 0, info->bi.fb->buffer_size);

    g_allocator = &allocator;
    g_allocator->read_efi_memory_map(
        info->bi.mmap,
        info->bi.mmap_size,
        info->bi.mmap_desc_size
    );

    auto* PML4 = static_cast<PageTable*>(g_allocator->request_page());
    memset(PML4, 0, 0x1000);
    ptm = PageTableManager(PML4);
    g_ptm = &ptm;

    const auto kernelStart = reinterpret_cast<uint64_t>(info->phys_start);
    const uint64_t kernelEnd = kernelStart + info->phys_size;
    const uint64_t kernelPages = (kernelEnd - kernelStart + 0xFFF) / 0x1000;

    g_allocator->lock_pages(
        reinterpret_cast<void*>(kernelStart),
        kernelPages
    );
    for (uint64_t phys = kernelStart; phys < kernelEnd; phys += 0x1000)
    {
        uint64_t virt = phys - kernelStart + KERNEL_VIRT_BASE;

        g_ptm->map_memory(
            reinterpret_cast<void*>(virt),
            reinterpret_cast<void*>(phys),
            (1ULL << PT_Flag::Present) |
            (1ULL << PT_Flag::ReadWrite) |
            (1ULL << PT_Flag::Global)
        );
    }

    void* kernelStackPhys =
        g_allocator->request_pages(KERNEL_STACK_PAGES);

    uint64_t kernelStackTop =
        KERNEL_VIRT_BASE + 0x8000000;

    for (uint64_t i = 0; i < KERNEL_STACK_PAGES; i++)
    {
        g_ptm->map_memory(
            reinterpret_cast<void*>(kernelStackTop - i * 0x1000),
            reinterpret_cast<void*>(
                reinterpret_cast<uint64_t>(kernelStackPhys) + i * 0x1000
            ),
            (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite)
        );
    }

    const uint64_t mMapEntries = info->bi.mmap_size / info->bi.mmap_desc_size;
    for (int i = 0; i < mMapEntries; i++)
    {
        auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<uint64_t>(info->bi.mmap) + (i *
            info->bi.mmap_desc_size));

        for (uint64_t addr = desc->phys_addr; addr < desc->phys_addr + desc->num_pages * 0x1000; addr += 0x1000)
        {
            g_ptm->map_memory(reinterpret_cast<void*>(addr), reinterpret_cast<void*>(addr),
                              (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite) | (1ULL << PT_Flag::Global));
        }
    }

    uint64_t bootstrap_start = reinterpret_cast<uint64_t>(&_PhysStart);
    uint64_t bootstrap_end = reinterpret_cast<uint64_t>(&_PhysEnd);

    for (uint64_t phys = bootstrap_start; phys < bootstrap_end; phys += 0x1000)
    {
        g_ptm->map_memory((void*)phys, (void*)phys,
                          (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite));

        uint64_t virt = phys + KERNEL_VIRT_BASE;
        g_ptm->map_memory((void*)virt, (void*)phys,
                          (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite));
    }

    auto fb_base = reinterpret_cast<uint64_t>(info->bi.fb->base_address);
    uint64_t fb_size = info->bi.fb->buffer_size + 0x1000;
    g_allocator->lock_pages(reinterpret_cast<void*>(fb_base), fb_size / 0x1000 + 1);
    for (uint64_t t = fb_base; t < fb_base + fb_size; t += 0x1000)
    {
        g_ptm->map_memory(reinterpret_cast<void*>(t), reinterpret_cast<void*>(t),
                          (1ULL << PT_Flag::WriteThrough) | // PWT=1
                          (1ULL << PT_Flag::Global));
    }

    auto bootInfoPhys = reinterpret_cast<uint64_t>(info);
    g_ptm->map_memory(reinterpret_cast<void*>(bootInfoPhys),
                      reinterpret_cast<void*>(bootInfoPhys),
                      (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite) | (1ULL << PT_Flag::Global));


    enter_higher_half(
        reinterpret_cast<uint64_t>(g_ptm->PML4),
        kernelStackTop,
        info->kernel_entry,
        &info->bi
    );
}
