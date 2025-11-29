//
// Created by linus on 05.10.24.
//

#ifndef INTERRUPTS_INTERNAL_H
#define INTERRUPTS_INTERNAL_H
#include <cstdint>

#define IRQ_SPURIOUS         0xFF
#define IRQ_TIMER            0x20
#define IRQ_PANIC            0xFE
#define IRQ_COMMON_STUB      0x31

// x86_64 Interrupt Frame Structure (pushed via asm stubs)
struct trap_frame {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

    // Error code (either from CPU or dummy 0)
    uint64_t error_code;

    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));


// Asm Stubs
extern "C" void isr_divide_error();
extern "C" void isr_invalid_opcode();
extern "C" void isr_double_fault();
extern "C" void isr_segment_not_present();
extern "C" void isr_stack_fault();
extern "C" void isr_gp_fault();
extern "C" void isr_page_fault();
extern "C" void isr_machine_check();
extern "C" void isr_keyboard_int();
extern "C" void isr_mouse_int();
extern "C" void isr_apic_timer_int();
extern "C" void isr_spurious_int();
extern "C" void isr_panic_ipi();

// C++ Handler
extern "C" void divide_error_handler(const trap_frame* frame);
extern "C" void invalid_opcode_handler(const trap_frame* frame);
extern "C" void double_fault_handler(const trap_frame* frame);
extern "C" void segment_not_present_handler(const trap_frame* frame);
extern "C" void stack_fault_handler(const trap_frame* frame);
extern "C" void gp_fault_handler(const trap_frame* frame);
extern "C" void page_fault_handler(trap_frame* frame);
extern "C" void machine_check_handler(const trap_frame* frame);
extern "C" void keyboard_int_handler(trap_frame* frame);
extern "C" void mouse_int_handler(trap_frame* frame);
extern "C" void apic_timer_int_handler(trap_frame* frame);
extern "C" void spurious_int_handler(trap_frame* frame);
extern "C" [[noreturn]] void panic_ipi_handler(trap_frame* frame);

#endif //INTERRUPTS_H