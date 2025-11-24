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
#include "../../../kernel/debug/fault_logger.h"

using kernel::debug::FaultContext;
using kernel::debug::FaultType;


static FaultContext make_fault_context(const interrupt_frame* frame) {
    FaultContext ctx{};
    ctx.rip        = frame->rip;
    ctx.cs         = frame->cs;
    ctx.rsp        = frame->rsp;
    ctx.rflags     = frame->rflags;
    ctx.error_code = frame->error_code;
    return ctx;
}

__attribute__((interrupt)) void page_fault_handler(interrupt_frame *frame) {
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_page_fault_detail(fault_addr, ctx.error_code, ctx);

    panic("Page fault");
   // kernel::SystemManager::system_panic("Page fault detected", -EPAGEFAULT);
}

__attribute__((interrupt)) void double_fault_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DoubleFault, ctx, "Double fault detected");
    kernel::SystemManager::system_panic("Double fault detected", -EDOUBLEFAULT);
}

__attribute__((interrupt)) void gp_fault_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::GeneralProtection, ctx, "General protection fault detected");

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
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_invalid_opcode_bytes(frame->rip, ctx);

    kernel::SystemManager::system_panic("Invalid opcode detected", -EINVOP);
    while (true);
}

// Stack Segment Fault (Vector 12)
__attribute__((interrupt)) void stack_fault_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::StackFault, ctx, "Stack fault detected");

    uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Stack selector: 0x%x", selector);

    kernel::SystemManager::system_panic("Stack fault detected", -ESTACKFAULT);
    while (true);
}

// Segment Not Present (Vector 11)
__attribute__((interrupt)) void segment_not_present_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::SegmentNotPresent, ctx, "Segment not present");

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
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DivideByZero, ctx, "Divide by zero");

    kernel::SystemManager::system_panic("Divide by zero", -EDIVZERO);
    while (true);
}

// Machine Check Exception (Vector 18)
__attribute__((interrupt)) void machine_check_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::MachineCheck, ctx, "Machine check exception");

    kernel::SystemManager::system_panic("Machine check exception", -EMACHCHECK);
    while (true);
}

// Generic unhandled interrupt handler
__attribute__((interrupt)) void unhandled_interrupt_handler(interrupt_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::UnhandledInterrupt, ctx, "Unhandled interrupt");

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
