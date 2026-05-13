//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_INTERNAL_H
#define INTERRUPTS_INTERNAL_H
#include <vespera/interrupts.h>
#include <vespera/types.h>

constexpr u8 DIVIDE_ERROR = 0x00;
constexpr u8 INVALID_OPCODE = 0x06;
constexpr u8 DOUBLE_FAULT = 0x08;
constexpr u8 SEGMENT_NOT_PRESENT = 0x0B;
constexpr u8 STACK_FAULT = 0x0C;
constexpr u8 GENERAL_PROTECTION = 0x0D;
constexpr u8 PAGE_FAULT = 0x0E;
constexpr u8 MACHINE_CHECK = 0x12;

constexpr u8 KEYBOARD = 0x21;
constexpr u8 MOUSE = 0x22;

constexpr u8 IRQ_SPURIOUS = 0xFF;
constexpr u8 IRQ_TIMER = 0x20;
constexpr u8 IRQ_PANIC = 0xFE;
constexpr u8 IRQ_COMMON_STUB = 0x31;
constexpr u8 IRQ_YIELD = 0x23;

extern "C" void (*isr_stub_table[256])();

#endif  // INTERRUPTS_H