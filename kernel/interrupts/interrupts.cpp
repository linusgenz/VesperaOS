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

#include <cstdint>
#include "../../arch/x86_64/interrupts/pic.h"
#include  "../../arch/x86_64/interrupts/apic.h"
#include "../../arch/x86_64/interrupts/ioapic.h"
#include "../../arch/x86_64/interrupts/idt.h"
#include "../../arch/x86_64/interrupts/interrupts_internal.h"
#include "../../include/log.h"
#include "../include/memory.h"
#include "../cpu/io.h"

namespace kernel::interrupts {
    void initialize() {
        arch::x86_64::interrupts::apic::apic_ticks[MAX_CPU_CORES-1] = 0;
        memset(arch::x86_64::interrupts::idt::irq_handler_table, 0, sizeof(arch::x86_64::interrupts::idt::irq_handler_table));

        arch::x86_64::interrupts::idt::init_irq_table();

        kernel::memory::map_memory(g_localApicAddr,
                                   g_localApicAddr,
                                   (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));

        arch::x86_64::interrupts::idt::load_default_idt();
        arch::x86_64::interrupts::pic::remap();
        //  arch::x86_64::interrupts::ioapic::init();
        arch::x86_64::interrupts::apic::init(0); // bsp
    }

    void allocate_vector(uint8_t vector, irq_handler_t handler, void *cookie) {
        arch::x86_64::interrupts::idt::allocate_vector(vector, handler, cookie);
    }

    void free_vector(uint8_t vec) {
        arch::x86_64::interrupts::idt::free_vector(vec);
    }

    uint8_t get_free_vector() {
        return arch::x86_64::interrupts::idt::get_free_vector();
    }

    void set_vector(uint8_t vector, void *handler) {
        arch::x86_64::interrupts::idt::set_idt_gate(handler, vector, IDT_TA_InterruptGate, 0x08);
    }

    arch::x86_64::interrupts::idt::IDTR* get_idtr_address() {
        return arch::x86_64::interrupts::idt::get_idtr_address();
    }

    void lapic_init(uint32_t cpu_id) {
        arch::x86_64::interrupts::apic::init(cpu_id);
    }

    void lapic_write(uint32_t offset, uint32_t value) {
        arch::x86_64::interrupts::apic::write(offset, value);
    }

    uint32_t lapic_read(uint32_t offset) {
        return arch::x86_64::interrupts::apic::read(offset);
    }

    void lapic_wait_for_delivery() {
        arch::x86_64::interrupts::apic::wait_for_delivery();
    }

    uint64_t lapic_get_ticks(uint32_t cpu_id) {
        return arch::x86_64::interrupts::apic::apic_ticks[cpu_id];
    }

    uint32_t lapic_get_id() {
        return arch::x86_64::interrupts::apic::local_apic_get_id();
    }

    void lapic_send_eoi() {
        arch::x86_64::interrupts::apic::send_eoi();
    }


    void mask_pic() {
        outb(PIC1_DATA, 0b11111001); // = 0xFD → IRQ1 (keyboard) activated
        outb(PIC2_DATA, 0b11101111); // = 0xEF → IRQ12 (mouse) activated
    }
}
