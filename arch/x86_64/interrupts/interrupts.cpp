#include <vespera/interrupts.h>
#include <vespera/kerrno.h>
#include <vespera/system/system_manager.h>

#include "../../../drivers/ps2/keyboard/ps2_keyboard.h"
#include "../../../drivers/ps2/mouse/mouse.h"
#include <vespera/log.h>
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/cpu/io.h"
#include "../../../kernel/debug/fault_logger.h"
#include "../../../kernel/utils/panic.h"
#include "apic.h"
#include "interrupts_internal.h"
#include "pic.h"

using kernel::debug::FaultContext;
using kernel::debug::FaultType;

static FaultContext make_fault_context(const TrapFrame *frame) {
    FaultContext ctx{};
    ctx.rip = frame->rip;
    ctx.cs = frame->cs;
    ctx.rsp = frame->rsp;
    ctx.rbp = frame->rbp;
    ctx.rflags = frame->rflags;
    ctx.error_code = frame->error_code;
    return ctx;
}

void page_fault_handler(TrapFrame *frame) {
    u64 fault_addr = 0;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_page_fault_detail(fault_addr, ctx.error_code, ctx);

    panic("Page fault");
    // kernel::SystemManager::system_panic("Page fault detected", -EPAGEFAULT);
}

void double_fault_handler(const TrapFrame *frame) {
    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DoubleFault, ctx, "Double fault detected");
    kernel::SystemManager::system_panic("Double fault detected", -KEDOUBLEFAULT);
}

void gp_fault_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::GeneralProtection, ctx, "General protection fault detected");

    if (frame->error_code & 0x1) {
        Log::error("  External event caused fault");
    }
    if (frame->error_code & 0x2) {
        Log::error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::error("  LDT referenced");
    } else {
        Log::error("  GDT referenced");
    }

    const u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Selector: 0x%x", selector);

    //  debug_check();

    panic("General protection fault detected");
    kernel::SystemManager::system_panic("General protection fault detected", -KEGPF);
}

// Invalid Opcode Fault (Vector 6)
extern "C" void invalid_opcode_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_invalid_opcode_bytes(frame->rip, ctx);
    panic("Invalid opcode detected");
    kernel::SystemManager::system_panic("Invalid opcode detected", -KEINVOP);
}

// Stack Segment Fault (Vector 12)
void stack_fault_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::StackFault, ctx, "Stack fault detected");

    const u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Stack selector: 0x%x", selector);

    kernel::SystemManager::system_panic("Stack fault detected", -KESTACKFAULT);
}

// Segment Not Present (Vector 11)
void segment_not_present_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::SegmentNotPresent, ctx, "Segment not present");

    u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Missing segment selector: 0x%x", selector);

    if (frame->error_code & 0x2) {
        Log::error("  IDT referenced");
    } else if (frame->error_code & 0x4) {
        Log::error("  LDT referenced");
    } else {
        Log::error("  GDT referenced");
    }

    kernel::SystemManager::system_panic("Segment not present", -KESEGNOTPRES);
}

// Divide by Zero (Vector 0)
void divide_error_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DivideByZero, ctx, "Divide by zero");

    kernel::SystemManager::system_panic("Divide by zero", -KEDIVZERO);
}

// Machine Check Exception (Vector 18)
void machine_check_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::MachineCheck, ctx, "Machine check exception");

    kernel::SystemManager::system_panic("Machine check exception", -KEMACHCHECK);
}

// Generic unhandled interrupt handler
void unhandled_interrupt_handler(const TrapFrame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::UnhandledInterrupt, ctx, "Unhandled interrupt");

    kernel::SystemManager::system_panic("Unhandled interrupt", -KEUNHANDLED);
}

void keyboard_int_handler(TrapFrame *) {
    u8 scancode = inb(0x60);
    ps2::keyboard::handle_scancode(scancode);
    arch::x86_64::interrupts::pic::end_master();
}

void mouse_int_handler(TrapFrame *) {
    u8 data = inb(0x60);
    input::mouse::handle_byte(data);
    arch::x86_64::interrupts::pic::end_slave();
}

void apic_timer_int_handler(TrapFrame *frame) {
    arch::x86_64::interrupts::apic::timer_accounting();
    arch::x86_64::interrupts::apic::send_eoi();
    arch::x86_64::interrupts::apic::timer_tick(frame);

    static u64 cursor_tick = 0;
    if (++cursor_tick % 500 == 0) {
        kernel::SystemManager::get_system_terminal()->tick_cursor();
    }
}

void spurious_int_handler(TrapFrame *) {
    Log::ok("SPURIOUS INTERRUPT");
}

[[noreturn]] void panic_ipi_handler(TrapFrame *) {
    u32 apic_id = arch::x86_64::interrupts::apic::local_apic_get_id();
    cpu_manager::halt_cpu(apic_id);
    while (true) asm volatile("cli; hlt");
}
