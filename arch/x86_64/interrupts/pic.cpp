// pic.cpp
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

#include "pic.h"
#include "../../../kernel/cpu/io.h"

namespace arch::x86_64::interrupts::pic {
    void initialize()
    {
        // ICW1: start initialization, ICW4 needed
        outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
        outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

        // ICW2: interrupt vector address
        outb(PIC1_DATA, IRQ_BASE);
        outb(PIC2_DATA, IRQ_BASE + 8);

        // ICW3: master/slave wiring
        outb(PIC1_DATA, 4);
        outb(PIC2_DATA, 2);

        // ICW4: 8086 mode, not special fully nested, not buffered, normal EOI
        outb(PIC1_DATA, ICW4_8086);
        outb(PIC2_DATA, ICW4_8086);

        // OCW1: Disable all IRQs
        outb(PIC1_DATA, 0xff);
        outb(PIC2_DATA, 0xff);
    }

    void disable() {
        outb(PIC1_DATA, 0xff);
        outb(PIC2_DATA, 0xff);
    }

    void end_master() {
        outb(PIC1_COMMAND, PIC_EOI);
    }

    void end_slave() {
        outb(PIC2_COMMAND, PIC_EOI);
        outb(PIC1_COMMAND, PIC_EOI);
    }

    void remap() {
        uint8_t a1, a2;

        a1 = inb(PIC1_DATA);
        io_wait();
        a2 = inb(PIC2_DATA);
        io_wait();

        outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();
        outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
        io_wait();

        outb(PIC1_DATA, 0x20);
        io_wait();
        outb(PIC2_DATA, 0x28);
        io_wait();

        outb(PIC1_DATA, 4);
        io_wait();
        outb(PIC2_DATA, 2);
        io_wait();

        outb(PIC1_DATA, ICW4_8086);
        io_wait();
        outb(PIC2_DATA, ICW4_8086);
        io_wait();

        outb(PIC1_DATA, a1);
        io_wait();
        outb(PIC2_DATA, a2);
    }
}