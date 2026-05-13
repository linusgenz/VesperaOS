// pit.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.04.26.
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

#include "pit.h"

#include <vespera/cpu/io.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>

//#include <arch/x86_64/interrupts/ioapic.h>
#include "../arch/x86_64/interrupts/ioapic.h" // TODO
#include "acpi/madt.h"

namespace kernel::time {

    u16 PitClock::latch_read() {
        // Issue a counter-latch command to channel 0 (freezes the counter snapshot).
        outb(PIT_PORT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LATCH | PIT_CMD_BINARY);

        const u8 lo = inb(PIT_PORT_CHANNEL0);
        const u8 hi = inb(PIT_PORT_CHANNEL0);
        return static_cast<u16>(lo) | (static_cast<u16>(hi) << 8);
    }

    static Irqreturn pit_irq_handler(void* cookie) {
        static_cast<PitClock*>(cookie)->on_irq();
        return IRQ_HANDLED;
    }

    int PitClock::init() {
        outb(PIT_PORT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOH | PIT_CMD_MODE2 | PIT_CMD_BINARY);

        outb(PIT_PORT_CHANNEL0, PIT_DIVISOR & 0xFF);
        outb(PIT_PORT_CHANNEL0, PIT_DIVISOR >> 8 & 0xFF);

        const u8 bsp = static_cast<u8>(acpi::madt::bsp_apic_id());

        const u8 vector = interrupts::get_free_vector();
        if (vector == 0xFF) {
            Log::error("[PIT] No free IDT vector for IRQ");
            return -1;
        }

        kernel::interrupts::allocate_vector(vector, &pit_irq_handler, this);
        arch::x86_64::interrupts::ioapic::configure_irq(arch::x86_64::interrupts::ioapic::PIT_ISA_IRQ, vector, bsp);

        available_ = true;

        Log::ok("[PIT ] Initialised: %llu Hz (divisor %u)", PIT_TICK_HZ, PIT_DIVISOR);
        return 0;
    }

    u64 PitClock::read_ticks() {
        const u64 coarse = irq_ticks_ * static_cast<u64>(PIT_DIVISOR);
        const u16 hw = latch_read();
        // hw counts down from PIT_DIVISOR; clamp to divisor in case of a race
        // between the latch read and the coarse increment.
        const u16 fine = (hw < PIT_DIVISOR) ? static_cast<u16>(PIT_DIVISOR - hw) : 0;
        return coarse + static_cast<u64>(fine);
    }

    u64 PitClock::read_ns() {
        const u64 ticks = read_ticks();
        return (ticks * 838'096ULL) / 1'000'000ULL;
    }

    void PitClock::on_irq() {
        irq_ticks_++;
    }

    void PitClock::busy_wait_us(const u32 us) {
        // Cap at one full counter period to avoid 16-bit rollover complexity.
        // For longer delays the caller should loop.
        constexpr u32 MAX_US = 54'000;  // ~54 ms max for a 16-bit 1.193 MHz counter
        const u32 clamped_us = (us > MAX_US) ? MAX_US : us;

        const u32 counts = (static_cast<u32>(PIT_BASE_FREQ_HZ) * clamped_us + 999'999U) / 1'000'000U;
        const u16 reload = (counts > 0xFFFF) ? 0xFFFF : static_cast<u16>(counts);

        // Gate off channel 2 (bit 0 of port 0x61) before loading the count.
        u8 gate = inb(PIT_PORT_GATE);
        gate &= ~0x01u;  // clear GATE2
        outb(PIT_PORT_GATE, gate);

        // Programme channel 2: mode 0 (interrupt on terminal count), access lo+hi.
        outb(PIT_PORT_COMMAND, PIT_CMD_CHANNEL2 | PIT_CMD_ACCESS_LOH | PIT_CMD_MODE0 | PIT_CMD_BINARY);

        outb(PIT_PORT_CHANNEL2, static_cast<u8>(reload & 0xFF));
        outb(PIT_PORT_CHANNEL2, static_cast<u8>((reload >> 8) & 0xFF));

        // Enable the gate to start counting.
        gate |= 0x01u;
        outb(PIT_PORT_GATE, gate);

        // Spin until OUT2 (bit 5 of port 0x61) goes high.
        while (!(inb(PIT_PORT_GATE) & 0x20u)) {
            asm volatile("pause");
        }

        // Disable gate to leave channel 2 in a known idle state.
        gate = inb(PIT_PORT_GATE);
        gate &= ~0x01u;
        outb(PIT_PORT_GATE, gate);
    }

}  // namespace kernel::time