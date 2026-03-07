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

#include <vespera/types.h>
#include "../../arch/x86_64/interrupts/idt.h"

namespace kernel::interrupts
{
    void initialize(); // sets IDT, APIC, IOAPIC, PIC
    bool allocate_vector(u8 vector, irq_handler_t handler, void* cookie = nullptr);
    u8 get_free_vector();
    void free_vector(u8 irqno);
    /**
 * @brief Finds a block of contiguous IRQ vectors.
 *
 * @param size Number of contiguous vectors required
 * @return u8 Start vector of the block or 0xFF if no block is available
 */
    u8 get_free_vector_block(usize size);
    arch::x86_64::interrupts::idt::IDTR* get_idtr_address();
    void lapic_send_eoi();
    void lapic_init(u32 cpu_id);
    void lapic_write(u32 offset, u32 value);
    u32 lapic_read(u32 offset);
    void lapic_wait_for_delivery();
    u64 lapic_get_ticks(u32 cpu_id);
    u32 lapic_get_id();
    void mask_pic();
}

#endif //INTERRUPTS_H
