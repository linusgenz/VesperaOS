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

#include <vespera/boot/boot.h>
#include <vespera/kernel_utils.h>
#include <vespera/mm/memory.h>

#include "arch/x86_64/cpu/msr.h"

namespace kernel::memory {
    void initialize_memory(BootInfo* boot_info) {
        initialize_page_frame_allocator(boot_info->m_map, boot_info->m_map_size, boot_info->m_map_desc_size);

        const u64 kernel_phys_start =
            reinterpret_cast<u64>(&kernel_start) - boot_info->kernel_virt_base + boot_info->kernel_phys_base;
        const u64 kernel_phys_end =
            reinterpret_cast<u64>(&kernel_end) - boot_info->kernel_virt_base + boot_info->kernel_phys_base;
        const u64 kernel_pages = (kernel_phys_end - kernel_phys_start) / 4096 + 1;

        lock_pages(make_phys(kernel_phys_start), kernel_pages);

        initialize_page_table_manager(boot_info);

        // PAT
        constexpr u32 ia32_pat_msr = 0x277;
        u64 pat_value = (0x06ULL << 0) |   // PAT0: WB
                        (0x01ULL << 8) |   // PAT1: WC
                        (0x07ULL << 16) |  // PAT2: UC-
                        (0x00ULL << 24) |  // PAT3: UC
                        (0x06ULL << 32) |  // PAT4: WB
                        (0x04ULL << 40) |  // PAT5: WT
                        (0x07ULL << 48) |  // PAT6: UC-
                        (0x05ULL << 56);   // PAT7: WP
        wrmsr(ia32_pat_msr, pat_value);

        const u64 fb_virt = reinterpret_cast<u64>(boot_info->framebuffer->base_address);
        const u64 fb_phys = boot_info->framebuffer->phys_base_address;
        const u64 fb_size = boot_info->framebuffer->buffer_size + 0x1000;

        lock_pages(make_phys(fb_phys), fb_size / 0x1000 + 1);

        for (u64 offset = 0; offset < fb_size; offset += 0x1000) {
            map_memory(
                virt_from_raw(fb_virt + offset),
                make_phys(fb_phys + offset),
                (1ULL << PtFlag::ReadWrite) | (1ULL << PtFlag::WriteThrough) | (1ULL << PtFlag::Global)
            );
        }

        asm volatile("mov %0, %%cr3" : : "r"(get_pagetable_address()) : "memory");
        g_hhdm_offset = boot_info->hhdm_offset;
        relocate_bitmap_to_hhdm();
    }
}  // namespace kernel::memory
