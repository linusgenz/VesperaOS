#include <kerrno.h>

#include "interrupts_internal.h"
#include "../../../kernel/utils/panic.h"
#include "apic.h"
#include "../../../kernel/include/scheduling.h"
#include "../../../include/log.h"
#include "../../../kernel/cpu/io.h"
#include "pic.h"
#include "../../../drivers/ps2/keyboard/ps2_keyboard.h"
#include "../../../drivers/ps2/mouse/mouse.h"
#include "../../../drivers/ps2/mouse/ps2_mouse.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/include/time.h"
#include "../../../kernel/system/system_manager.h"


__attribute__((interrupt)) void page_fault_handler(interrupt_frame *frame) {
    // Hole CR2 Register (Page Fault Address)
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    // Hole Error Code
    uint64_t error_code = frame->error_code;

    Log::Error("PAGE FAULT: addr=0x%llx, error=0x%llx, rip=0x%llx",
               fault_addr, error_code, frame->rip);

    // Dekodiere Error Code
    Log::Error("  Present: %s, Write: %s, User: %s, Reserved: %s",
               (error_code & 1) ? "Yes" : "No",
               (error_code & 2) ? "Yes" : "No",
               (error_code & 4) ? "Yes" : "No",
               (error_code & 8) ? "Yes" : "No");

    kernel::SystemManager::system_panic("Page fault detected", -EPAGEFAULT);
}

__attribute__((interrupt)) void double_fault_handler(interrupt_frame *frame) {
    Log::Error("DOUBLE FAULT: rip=0x%llx, error=0x%llx",
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);
    kernel::SystemManager::system_panic("Double fault detected", -EDOUBLEFAULT);
}

__attribute__((interrupt)) void gp_fault_handler(interrupt_frame *frame) {
    Log::Error("GENERAL PROTECTION FAULT: rip=0x%llx, error=0x%llx",
               frame->rip, frame->error_code);

    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    if (frame->error_code & 0x1) {
        Log::Error("  External event caused fault");
    }
    if (frame->error_code & 0x2) {
        Log::Error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::Error("  LDT referenced");
    } else {
        Log::Error("  GDT referenced");
    }

    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Selector: 0x%x", selector);

    kernel::SystemManager::system_panic("General protection fault detected", -EGPF);
}

// Invalid Opcode Fault (Vector 6)
__attribute__((interrupt)) void invalid_opcode_handler(interrupt_frame *frame) {
    Log::Error("INVALID OPCODE: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    // Zeige die Bytes an der fehlerhaften Adresse
    uint8_t *opcode_ptr = (uint8_t *) frame->rip;
    Log::Error("  Opcode bytes: %02x %02x %02x %02x",
               opcode_ptr[0], opcode_ptr[1], opcode_ptr[2], opcode_ptr[3]);

    kernel::SystemManager::system_panic("Invalid opcode detected", -EINVOP);
    while (true);
}

// Stack Segment Fault (Vector 12)
__attribute__((interrupt)) void stack_fault_handler(interrupt_frame *frame) {
    Log::Error("STACK FAULT: rip=0x%llx, error=0x%llx",
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Stack selector: 0x%x", selector);

    kernel::SystemManager::system_panic("Stack fault detected", -ESTACKFAULT);
    while (true);
}

// Segment Not Present (Vector 11)
__attribute__((interrupt)) void segment_not_present_handler(interrupt_frame *frame) {
    Log::Error("SEGMENT NOT PRESENT: rip=0x%llx, error=0x%llx",
               frame->rip, frame->error_code);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Missing segment selector: 0x%x", selector);

    if (frame->error_code & 0x2) {
        Log::Error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::Error("  LDT referenced");
    } else {
        Log::Error("  GDT referenced");
    }

    kernel::SystemManager::system_panic("Segment not present", -ESEGNOTPRES);
    while (true);
}

// Divide by Zero (Vector 0)
__attribute__((interrupt)) void divide_error_handler(interrupt_frame *frame) {
    Log::Error("DIVIDE BY ZERO: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    kernel::SystemManager::system_panic("Divide by zero", -EDIVZERO);
    while (true);
}

// Machine Check Exception (Vector 18)
__attribute__((interrupt)) void machine_check_handler(interrupt_frame *frame) {
    Log::Error("MACHINE CHECK EXCEPTION: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    kernel::SystemManager::system_panic("Machine check exception", -EMACHCHECK);
    while (true);
}

// Generic unhandled interrupt handler
__attribute__((interrupt)) void unhandled_interrupt_handler(interrupt_frame *frame) {
    Log::Error("UNHANDLED INTERRUPT: rip=0x%llx", frame->rip);
    Log::Error("  CS=0x%llx, RSP=0x%llx, RFLAGS=0x%llx",
               frame->cs, frame->rsp, frame->rflags);

    kernel::SystemManager::system_panic("Unhandled interrupt", -EUNHANDLED);
    while (true);
}

__attribute__((interrupt)) void keyboard_int_handler(interrupt_frame *frame) {
    uint8_t scancode = inb(0x60);
    ps2::keyboard::handle_byte(scancode);
    arch::x86_64::interrupts::pic::end_master();
}

__attribute__((interrupt)) void mouse_int_handler(interrupt_frame *frame) {
    global_renderer->print("mouse_int_handler");
    uint8_t data = inb(0x60);
    input::mouse::handle_byte(data);
    arch::x86_64::interrupts::pic::end_slave();
}

__attribute__((interrupt))
void apic_timer_int_handler(interrupt_frame *frame) {
    arch::x86_64::interrupts::apic::timer_accounting();
    arch::x86_64::interrupts::apic::send_eoi();
    arch::x86_64::interrupts::apic::timer_tick(frame);
}

__attribute__((interrupt))
void spurious_int_handler(interrupt_frame *frame) {
    Log::Ok("SPURIOUS INTERRUPT");
}

__attribute__((interrupt))
[[noreturn]] void panic_ipi_handler(interrupt_frame* frame) {
    uint32_t apic_id = arch::x86_64::interrupts::apic::local_apic_get_id();
    CPUManager::halt_cpu(apic_id);
    while(true) asm volatile("cli; hlt");
}
