#include <kernel/interrupts.h>
#include <kernel/kerrno.h>

#include "interrupts_internal.h"
#include "../../../kernel/utils/panic.h"
#include "apic.h"
#include "../../../include/log.h"
#include "../../../kernel/cpu/io.h"
#include "pic.h"
#include "../../../drivers/ps2/keyboard/ps2_keyboard.h"
#include "../../../drivers/ps2/mouse/mouse.h"
#include "../../../kernel/cpu/cpu_manager.h"
#include <kernel/system/system_manager.h>
#include "../../../kernel/debug/fault_logger.h"
#include "../gdt/gdt.h"

using kernel::debug::FaultContext;
using kernel::debug::FaultType;


static FaultContext make_fault_context(const trap_frame* frame) {
    FaultContext ctx{};
    ctx.rip        = frame->rip;
    ctx.cs         = frame->cs;
    ctx.rsp        = frame->rsp;
    ctx.rbp        = frame->rbp;
    ctx.rflags     = frame->rflags;
    ctx.error_code = frame->error_code;
    return ctx;
}


void page_fault_handler(trap_frame *frame) {
    uint64_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_page_fault_detail(fault_addr, ctx.error_code, ctx);

    panic("Page fault");
   // kernel::SystemManager::system_panic("Page fault detected", -EPAGEFAULT);
}

void double_fault_handler(const trap_frame *frame) {
    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DoubleFault, ctx, "Double fault detected");
    kernel::SystemManager::system_panic("Double fault detected", -KEDOUBLEFAULT);
}










static void dump_desc(uint16_t selector) {
    GDTPtr gdtr;
    asm volatile("sgdt %0" : "=m"(gdtr));
    uint16_t index = selector >> 3;
    uint64_t gdt_base = gdtr.base;
    uint32_t desc_off = index * 8;

    Log::debug("GDTR: base=0x%016llx limit=0x%04x\n", (unsigned long long)gdt_base, gdtr.limit);
    Log::debug("Selector=0x%04x index=%u offset=0x%x\n", selector, index, desc_off);

    // Limit check
    if ((uint32_t)(desc_off + 7) > gdtr.limit) {
        Log::debug(">> OUT OF GDT LIMIT: desc_off+7 (0x%x) > limit (0x%x)\n", desc_off+7, gdtr.limit);
        return;
    }

    uint8_t *d = (uint8_t*)(uintptr_t)(gdt_base + desc_off);
    Log::debug("Descriptor bytes: ");
    for (int i = 0; i < 8; ++i) Log::Print("%02x ", d[i]);
    Log::PrintLn("");

    uint8_t limit_low = d[0];
    uint8_t limit_mid = d[1];
    uint8_t base_low = d[2];
    uint8_t base_mid = d[3];
    uint8_t base_high = d[4];
    uint8_t access = d[5];
    uint8_t flags_limit = d[6];
    uint8_t base_upper = d[7];

    uint16_t limit_high = (flags_limit & 0x0F);
    uint32_t descriptor_limit = (limit_high << 16) | (limit_mid << 8) | limit_low;

    uint8_t present = (access >> 7) & 1;
    uint8_t dpl = (access >> 5) & 3;
    uint8_t s = (access >> 4) & 1; // 1=data/code, 0=system
    uint8_t type = access & 0x0F;

    uint8_t L = (flags_limit >> 5) & 1;
    uint8_t DB = (flags_limit >> 6) & 1;
    uint8_t G = (flags_limit >> 7) & 1;

    Log::debug("Decoded: present=%u dpl=%u S=%u type=0x%x L=%u D/B=%u G=%u limit=0x%x\n",
           present, dpl, s, type, L, DB, G, descriptor_limit);

    // Interpret type bits for code/data
    if (s == 0) {
        Log::debug("Descriptor is a SYSTEM descriptor (S=0)\n");
    } else {
        uint8_t exec = (type >> 3) & 1;
        uint8_t conforming = (type >> 2) & 1;
        uint8_t readable = (type >> 1) & 1;
        uint8_t writable = (type >> 1) & 1; // same bit position but meaning depends on exec
        Log::debug(" Type bits: exec=%u conforming=%u readable/writable=%u\n", exec, conforming, readable);
    }

    // Basic invalid combos
    if (L && DB) Log::debug(">> INVALID: both L and D/B are set (not allowed for code seg)\n");
    if (!present) Log::debug(">> NOT PRESENT (P=0)\n");

    // Print current CS/CPL and RPL for diagnostics
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    uint16_t cpl = cs & 3;
    Log::debug("Current CS=0x%04x CPL=%u\n", cs, cpl);

    uint8_t rpl = selector & 3;
    Log::debug("Selector RPL=%u\n", rpl);

    // More nuanced DPL/CPL/RPL checks:
    uint8_t exec = (type >> 3) & 1;
    if (exec) {
        // code segment
        if ((type >> 2) & 1) {
            Log::debug("Code descriptor is conforming.\n");
            // For conforming code: DPL must be <= RPL? see intel manual; we'll print values
        } else {
            Log::debug("Code descriptor is non-conforming.\n");
        }
    } else {
        Log::debug("Data descriptor: writable=%u\n", (type>>1)&1);
    }
}

// Example usage in kernel debug
void debug_check(void) {
    // check user CS/SS selectors commonly 0x2B / 0x23
    dump_desc(0x2b); // CS
    dump_desc(0x23); // SS
}







void gp_fault_handler(const trap_frame *frame) {
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


  //  debug_check();

    panic("General protection fault detected");
    kernel::SystemManager::system_panic("General protection fault detected", -KEGPF);
}


// Invalid Opcode Fault (Vector 6)
extern "C"  void invalid_opcode_handler(const trap_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_invalid_opcode_bytes(frame->rip, ctx);
    panic("Invalid opcode detected");
    kernel::SystemManager::system_panic("Invalid opcode detected", -KEINVOP);
}

// Stack Segment Fault (Vector 12)
void stack_fault_handler(const trap_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::StackFault, ctx, "Stack fault detected");

    const uint16_t selector = (frame->error_code >> 3) & 0x1FFF;
    Log::Error("  Stack selector: 0x%x", selector);

    kernel::SystemManager::system_panic("Stack fault detected", -KESTACKFAULT);
}

// Segment Not Present (Vector 11)
void segment_not_present_handler(const trap_frame *frame) {
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

    kernel::SystemManager::system_panic("Segment not present", -KESEGNOTPRES);
}

// Divide by Zero (Vector 0)
void divide_error_handler(const trap_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DivideByZero, ctx, "Divide by zero");

    kernel::SystemManager::system_panic("Divide by zero", -KEDIVZERO);
}

// Machine Check Exception (Vector 18)
void machine_check_handler(const trap_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::MachineCheck, ctx, "Machine check exception");

    kernel::SystemManager::system_panic("Machine check exception", -KEMACHCHECK);
}

// Generic unhandled interrupt handler
void unhandled_interrupt_handler(const trap_frame *frame) {
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::UnhandledInterrupt, ctx, "Unhandled interrupt");

    kernel::SystemManager::system_panic("Unhandled interrupt", -KEUNHANDLED);
}

void keyboard_int_handler(trap_frame *frame) {
    uint8_t scancode = inb(0x60);
    ps2::keyboard::handle_scancode(scancode);
    arch::x86_64::interrupts::pic::end_master();
}

void mouse_int_handler(trap_frame *frame) {
    global_renderer->print("mouse_int_handler");
    uint8_t data = inb(0x60);
    input::mouse::handle_byte(data);
    arch::x86_64::interrupts::pic::end_slave();
}


void apic_timer_int_handler(trap_frame *frame) {
    arch::x86_64::interrupts::apic::timer_accounting();
    arch::x86_64::interrupts::apic::send_eoi();
    arch::x86_64::interrupts::apic::timer_tick(frame);
}

void spurious_int_handler(trap_frame *frame) {
    Log::Ok("SPURIOUS INTERRUPT");
}

[[noreturn]] void panic_ipi_handler(trap_frame* frame) {
    uint32_t apic_id = arch::x86_64::interrupts::apic::local_apic_get_id();
    CPUManager::halt_cpu(apic_id);
    while(true) asm volatile("cli; hlt");
}
