// interrupts.h
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

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <cstdint>
#include "../../arch/x86_64/interrupts/idt.h"

namespace kernel::interrupts {
    void initialize();  // sets IDT, APIC, IOAPIC, PIC
    bool allocate_vector(uint8_t vector, irq_handler_t handler, void* cookie = nullptr);
    uint8_t get_free_vector();
    void free_vector(uint8_t irqno);
    arch::x86_64::interrupts::idt::IDTR* get_idtr_address();
    void lapic_send_eoi();
    void lapic_init(uint32_t cpu_id);
    void lapic_write(uint32_t offset, uint32_t value);
    uint32_t lapic_read(uint32_t offset);
    void lapic_wait_for_delivery();
    uint64_t lapic_get_ticks(uint32_t cpu_id);
    uint32_t lapic_get_id();
    void mask_pic();
}

#endif //INTERRUPTS_H
