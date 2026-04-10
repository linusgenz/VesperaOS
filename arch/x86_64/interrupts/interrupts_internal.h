//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_INTERNAL_H
#define INTERRUPTS_INTERNAL_H
#include <vespera/types.h>
#include <vespera/interrupts.h>

#define IRQ_SPURIOUS 0xFF
#define IRQ_TIMER 0x20
#define IRQ_PANIC 0xFE
#define IRQ_COMMON_STUB 0x31
#define IRQ_YIELD        0x23

extern "C" void (*isr_stub_table[256])();

#endif  // INTERRUPTS_H