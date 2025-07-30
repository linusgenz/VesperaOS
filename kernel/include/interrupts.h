// interrupts.h
//
// LuminOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.07.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "stdint.h"
#include "../../arch/x86_64/interrupts/idt.h"

#define IRQ_XHCI_VECTOR      0x30

namespace kernel::interrupts {
    void initialize();  // sets IDT, APIC, IOAPIC, PIC
    void register_irq(uint8_t irq, irq_handler_t handler, void* cookie = nullptr);
    arch::x86_64::interrupts::idt::IDTR* get_idtr_address();
    void lapic_send_eoi();
    void lapic_init(uint32_t cpu_id);
    void lapic_write(uint32_t offset, uint32_t value);
    uint32_t lapic_read(uint32_t offset);
    void lapic_wait_for_delivery();
    uint64_t lapic_get_ticks(uint32_t cpu_id);
    uint32_t lapic_get_id();
    void set_vector(uint8_t vector, void *handler);
    void mask_pic();
}

#endif //INTERRUPTS_H
