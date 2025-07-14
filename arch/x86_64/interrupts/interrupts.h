//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_H
#define INTERRUPTS_H
#include <stdint.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

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

void pic_init();
void remap_pic();
void pic_end_master();
void pic_end_slave();
void pic_disable();

#endif //INTERRUPTS_H