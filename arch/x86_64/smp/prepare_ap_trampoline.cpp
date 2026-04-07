// prepare_ap_trampoline.cpp
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

#include "prepare_ap_trampoline.h"

#include <ap_trampoline_blob.h>
#include <vespera/interrupts.h>

#include "../interrupts/idt.h"
#include <vespera/mm/memory.h>
#include <klib/string.h>

#define TRAMPOLINE_VIRT 0x8000
#define IDTR_PHYS 0x1000
#define PML4_PHYS 0x2000
#define REPORT_PHYS 0x7000
#define ENTRY_PTR_PHYS 0x3000

extern "C" void ap_main();

void prepare_ap_trampoline() {
    for (u64 phys = 0x1000; phys <= 0x9000; phys += 0x1000) {
        kernel::memory::map_memory(
            virt_from_raw(phys),
            make_phys(phys),
            0
        );
        // HHDM map
        kernel::memory::map_memory(
            phys_to_virt(make_phys(phys)),
            make_phys(phys),
            0
        );
    }

    memcpy(reinterpret_cast<void*>(TRAMPOLINE_VIRT), ap_trampoline_bin, ap_trampoline_bin_len);

    *reinterpret_cast<u32*>(PML4_PHYS)    = static_cast<u32>(kernel::memory::get_pagetable_address());
    *reinterpret_cast<arch::x86_64::interrupts::idt::IDTR*>(IDTR_PHYS) = *kernel::interrupts::get_idtr_address();
    *reinterpret_cast<u64*>(ENTRY_PTR_PHYS) = reinterpret_cast<u64>(ap_main);

    asm volatile("wbinvd" ::: "memory");
}