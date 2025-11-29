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

#include <cstdint>
/*
#include <kernel/memory.h>
#include <kernel/interrupts.h>
*/
#include "../interrupts/idt.h"

void prepare_ap_trampoline() {
   // *(volatile uint64_t *) 0x2000 = kernel::memory::get_pagetable_address();
   // *(arch::x86_64::interrupts::idt::IDTR *) 0x1000 = *kernel::interrupts::get_idtr_address();
    //   __asm__ volatile("wbinvd" ::: "memory");
}