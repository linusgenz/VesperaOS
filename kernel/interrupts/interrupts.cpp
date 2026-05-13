// interrupts.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 30.07.25.
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

#include "../arch/x86_64/interrupts/apic.h"
#include "../arch/x86_64/interrupts/idt.h"
#include "../arch/x86_64/interrupts/ioapic.h"
#include "../arch/x86_64/interrupts/pic.h"
#include <vespera/cpu/io.h>

namespace kernel::interrupts {
    void initialize() {
        arch::x86_64::interrupts::idt::init_irq_table();
        arch::x86_64::interrupts::idt::load_default_idt();
        arch::x86_64::interrupts::pic::remap();
        arch::x86_64::interrupts::ioapic::init();
        arch::x86_64::interrupts::apic::init_bsp();
    }

    bool allocate_vector(const u8 vector, const irq_handler_t handler, void* cookie) {
        return arch::x86_64::interrupts::idt::allocate_vector(vector, handler, cookie);
    }

    void free_vector(const u8 vec) {
        arch::x86_64::interrupts::idt::free_vector(vec);
    }

    u8 get_free_vector_block(const usize size) {
        return arch::x86_64::interrupts::idt::get_free_vector_block(size);
    }

    u8 get_free_vector() {
        return arch::x86_64::interrupts::idt::get_free_vector();
    }

    void mask_pic() {
        outb(PIC1_DATA, 0b11111001);  // = 0xFD → IRQ1 (keyboard) activated
        outb(PIC2_DATA, 0b11111111);  // = 0xEF → IRQ12 (mouse) activated, demask 4th bit to enable mouse
    }
}  // namespace kernel::interrupts
