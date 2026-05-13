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

enum Irqreturn : int {
    IRQ_HANDLED = 1,
    IRQ_NONE = 0,
    IRQ_ERROR = -1
};

using irq_handler_t = Irqreturn (*)(void* cookie);

namespace arch::x86_64::interrupts::idt {
    struct IDTR;
}

struct TrapFrame {
    // Callee-saved + scratch registers
    u64 r15;
    u64 r14;
    u64 r13;
    u64 r12;
    u64 r11;
    u64 r10;
    u64 r9;
    u64 r8;
    u64 rbp;
    u64 rdi;
    u64 rsi;
    u64 rdx;
    u64 rcx;
    u64 rbx;
    u64 rax;

    u64 vector;
    u64 error_code;

    // IRET frame
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

static_assert(__builtin_offsetof(TrapFrame, r15)        == 0x00);
static_assert(__builtin_offsetof(TrapFrame, r14)        == 0x08);
static_assert(__builtin_offsetof(TrapFrame, r13)        == 0x10);
static_assert(__builtin_offsetof(TrapFrame, r12)        == 0x18);
static_assert(__builtin_offsetof(TrapFrame, r11)        == 0x20);
static_assert(__builtin_offsetof(TrapFrame, r10)        == 0x28);
static_assert(__builtin_offsetof(TrapFrame, r9)         == 0x30);
static_assert(__builtin_offsetof(TrapFrame, r8)         == 0x38);
static_assert(__builtin_offsetof(TrapFrame, rbp)        == 0x40);
static_assert(__builtin_offsetof(TrapFrame, rdi)        == 0x48);
static_assert(__builtin_offsetof(TrapFrame, rsi)        == 0x50);
static_assert(__builtin_offsetof(TrapFrame, rdx)        == 0x58);
static_assert(__builtin_offsetof(TrapFrame, rcx)        == 0x60);
static_assert(__builtin_offsetof(TrapFrame, rbx)        == 0x68);
static_assert(__builtin_offsetof(TrapFrame, rax)        == 0x70);
static_assert(__builtin_offsetof(TrapFrame, vector)     == 0x78);
static_assert(__builtin_offsetof(TrapFrame, error_code) == 0x80);
static_assert(__builtin_offsetof(TrapFrame, rip)        == 0x88);
static_assert(__builtin_offsetof(TrapFrame, cs)         == 0x90);
static_assert(__builtin_offsetof(TrapFrame, rflags)     == 0x98);
static_assert(__builtin_offsetof(TrapFrame, rsp)        == 0xA0);
static_assert(__builtin_offsetof(TrapFrame, ss)         == 0xA8);
static_assert(sizeof(TrapFrame)                         == 0xB0);

inline bool trap_frame_from_user(const TrapFrame* tf) {
    return (tf->cs & 3) == 3;
}

namespace kernel::interrupts {
    void initialize();  // sets IDT, APIC, IOAPIC, PIC
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

    void mask_pic();
}  // namespace kernel::interrupts

#endif  // INTERRUPTS_H
