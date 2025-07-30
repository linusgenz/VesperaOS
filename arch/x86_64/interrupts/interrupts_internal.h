//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_INTERNAL_H
#define INTERRUPTS_INTERNAL_H
#include <stdint.h>

#define IRQ_SPURIOUS         0xFF
#define IRQ_TIMER            0x20
#define IRQ_ERROR            0xFE
#define IRQ_COMMON_STUB      0x31

// x86_64 Interrupt Frame Structure
struct interrupt_frame {
    uint64_t rip;        // Instruction Pointer
    uint64_t cs;         // Code Segment
    uint64_t rflags;     // CPU Flags
    uint64_t rsp;        // Stack Pointer
    uint64_t ss;         // Stack Segment
    uint64_t error_code; // Error Code (nur bei bestimmten Exceptions)
};
// Standard Exception Handlers
__attribute__((interrupt)) void divide_error_handler(interrupt_frame* frame);
__attribute__((interrupt)) void invalid_opcode_handler(interrupt_frame* frame);
__attribute__((interrupt)) void double_fault_handler(interrupt_frame* frame);
__attribute__((interrupt)) void segment_not_present_handler(interrupt_frame* frame);
__attribute__((interrupt)) void stack_fault_handler(interrupt_frame* frame);
__attribute__((interrupt)) void gp_fault_handler(interrupt_frame* frame); // General protection fault
__attribute__((interrupt)) void page_fault_handler(interrupt_frame* frame);
__attribute__((interrupt)) void machine_check_handler(interrupt_frame* frame);
__attribute__((interrupt)) void unhandled_interrupt_handler(interrupt_frame* frame);

// Device Interrupt Handlers
__attribute__((interrupt)) void keyboard_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void mouse_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void apic_timer_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void spurious_int_handler(interrupt_frame* frame);
__attribute__((interrupt)) void ap_entry_int_handler(interrupt_frame* frame);

#endif //INTERRUPTS_H