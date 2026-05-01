#include <vespera/interrupts.h>
#include <vespera/kerrno.h>
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/system/system_manager.h>

#include "../../../drivers/ps2/keyboard/ps2_keyboard.h"
#include "../../../drivers/ps2/mouse/mouse.h"
#include <vespera/cpu/io.h>
#include "../../../kernel/cpu/cpu_manager.h"
#include "../../../kernel/debug/fault_logger.h"
#include "../../../kernel/scheduling/cpu_scheduler.h"
#include "../../../kernel/scheduling/unit_termination.h"
#include "../../../kernel/utils/panic.h"
#include "apic.h"
#include "interrupts_internal.h"
#include "pic.h"

using kernel::debug::FaultContext;
using kernel::debug::FaultType;

static FaultContext make_fault_context(const TrapFrame* frame) {
    FaultContext ctx{};
    ctx.rip = frame->rip;
    ctx.cs = frame->cs;
    ctx.rsp = frame->rsp;
    ctx.rbp = frame->rbp;
    ctx.rflags = frame->rflags;
    ctx.error_code = frame->error_code;
    return ctx;
}

void double_fault_handler(const TrapFrame* frame) {
    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DoubleFault, frame, "Double fault detected");
    kernel::SystemManager::system_panic("Double fault detected", -KEDOUBLEFAULT);
}

void page_fault_handler(TrapFrame* frame) {
    u64 fault_addr = 0;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));

    /*  if (frame->cs & 0x3) {
          Unit* u = kernel::scheduling::get_current_unit();
          Realm* realm = RealmManager::get(u->rid);

          const u64 stack_top    = virt_raw(u->context.user_stack_top);
          const u64 stack_bottom = stack_top - u->context.user_stack_size;

          const char* reason;
          if (fault_addr >= stack_bottom - PAGE_SIZE && fault_addr < stack_bottom) {
              reason = "stack overflow (guard page)";
          } else if (fault_addr < stack_bottom) {
              reason = "stack overflow (below stack)";
          } else if (fault_addr >= stack_top) {
              reason = "invalid stack access (above stack)";
          } else {
              reason = "segmentation fault";
          }

          if (realm) {
              Log::print_ln("[%llu]  %s (core dumped)  %s",
                  static_cast<u64>(realm->id), reason, realm->name);
          }

          signal_send(u, Signal::SIGSEGV);
          signal_dispatch(u, frame);
          __builtin_unreachable();
      }*/

    // Kernel-seitiger Page Fault
    FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_page_fault_detail(fault_addr, ctx.error_code, frame);
    kernel::SystemManager::system_panic("Page fault detected", -KEPAGEFAULT);
}

void gp_fault_handler(TrapFrame* frame) {
    /* if (frame->cs & 0x3) {
         Unit* u = kernel::scheduling::get_current_unit();
         Realm* realm = RealmManager::get(u->rid);
         if (realm) {
             Log::print_ln("[%llu]  segmentation fault (core dumped)  %s",
                 static_cast<u64>(realm->id), realm->name);
         }
         signal_send(u, Signal::SIGSEGV);
         signal_dispatch(u, frame);
         __builtin_unreachable();
     }*/

    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::GeneralProtection, frame, "General protection fault detected");

    if (frame->error_code & 0x1) Log::error("  External event caused fault");
    if (frame->error_code & 0x2)
        Log::error("  IDT referenced");
    else if (frame->error_code & 0x4)
        Log::error("  LDT referenced");
    else
        Log::error("  GDT referenced");

    const u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Selector: 0x%x", selector);

    kernel::SystemManager::system_panic("General protection fault detected", -KEGPF);
}

extern "C" void invalid_opcode_handler(TrapFrame* frame) {
    /* if (frame->cs & 0x3) {
         Unit* u = kernel::scheduling::get_current_unit();
         Realm* realm = RealmManager::get(u->rid);
         if (realm) {
             Log::print_ln("[%llu]  illegal instruction (core dumped)  %s",
                 static_cast<u64>(realm->id), realm->name);
         }
         signal_send(u, Signal::SIGILL);
         signal_dispatch(u, frame);
         __builtin_unreachable();
     }*/

    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_invalid_opcode_bytes(frame->rip, frame);
    kernel::SystemManager::system_panic("Invalid opcode detected", -KEINVOP);
}

void stack_fault_handler(TrapFrame* frame) {
    if (frame->cs & 0x3) {
        Unit* u = kernel::scheduling::get_current_unit();
        Realm* realm = u->parent;
        if (realm) {
            Log::print_ln("[%llu]  stack fault (core dumped)  %s", static_cast<u64>(realm->id), realm->name);
        }
        signal_send(u, Signal::SIGSEGV);
        signal_dispatch(u, frame);
        __builtin_unreachable();
    }

    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::StackFault, frame, "Stack fault detected");
    const u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Stack selector: 0x%x", selector);
    kernel::SystemManager::system_panic("Stack fault detected", -KESTACKFAULT);
}

void segment_not_present_handler(TrapFrame* frame) {
    if (frame->cs & 0x3) {
        Unit* u = kernel::scheduling::get_current_unit();
        Realm* realm = u->parent;
        if (realm) {
            Log::print_ln("[%llu]  bus error (core dumped)  %s", static_cast<u64>(realm->id), realm->name);
        }
        signal_send(u, Signal::SIGBUS);
        signal_dispatch(u, frame);
        __builtin_unreachable();
    }

    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::SegmentNotPresent, frame, "Segment not present");
    const u16 selector = (frame->error_code >> 3) & 0x1FFF;
    Log::error("  Missing segment selector: 0x%x", selector);
    if (frame->error_code & 0x2)
        Log::error("  IDT referenced");
    else if (frame->error_code & 0x4)
        Log::error("  LDT referenced");
    else
        Log::error("  GDT referenced");
    kernel::SystemManager::system_panic("Segment not present", -KESEGNOTPRES);
}

void divide_error_handler(TrapFrame* frame) {
    if (frame->cs & 0x3) {
        Unit* u = kernel::scheduling::get_current_unit();
        Realm* realm = u->parent;
        if (realm) {
            Log::print_ln(
                "[%llu]  floating point exception (core dumped)  %s", static_cast<u64>(realm->id), realm->name
            );
        }
        signal_send(u, Signal::SIGFPE);
        signal_dispatch(u, frame);
        __builtin_unreachable();
    }

    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::DivideByZero, frame, "Divide by zero");
    kernel::SystemManager::system_panic("Divide by zero", -KEDIVZERO);
}

// Machine Check Exception (Vector 18)
void machine_check_handler(const TrapFrame* frame) {
    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::MachineCheck, frame, "Machine check exception");

    kernel::SystemManager::system_panic("Machine check exception", -KEMACHCHECK);
}

// Generic unhandled interrupt handler
void unhandled_interrupt_handler(const TrapFrame* frame) {
    const FaultContext ctx = make_fault_context(frame);
    kernel::debug::log_fault(FaultType::UnhandledInterrupt, frame, "Unhandled interrupt");

    kernel::SystemManager::system_panic("Unhandled interrupt", -KEUNHANDLED);
}

void keyboard_int_handler(TrapFrame*) {
    const u8 scancode = inb(0x60);
    ps2::keyboard::handle_scancode(scancode);
    arch::x86_64::interrupts::pic::end_master();
}

void mouse_int_handler(TrapFrame*) {
    const u8 data = inb(0x60);
    input::mouse::handle_byte(data);
    arch::x86_64::interrupts::pic::end_slave();
}

void apic_timer_int_handler(TrapFrame* frame) {
    arch::x86_64::interrupts::apic::timer_accounting();
    arch::x86_64::interrupts::apic::send_eoi();

    if (frame->cs & 0x3) {
        Unit* u = kernel::scheduling::get_current_unit();
        if (u && u->is_user && u->state == UnitState::Running) signal_dispatch(u, frame);
    }

    arch::x86_64::interrupts::apic::timer_tick(frame);

    static u64 cursor_tick = 0;
    if (++cursor_tick % 500 == 0) {
        kernel::SystemManager::get_system_terminal()->tick_cursor();
    }
}

void spurious_int_handler(TrapFrame*) {
    Log::ok("SPURIOUS INTERRUPT");
}

[[noreturn]] void panic_ipi_handler(TrapFrame*) {
    u32 apic_id = arch::x86_64::interrupts::apic::local_apic_get_id();
    cpu_manager::halt_cpu(apic_id);
    while (true) asm volatile("cli; hlt");
}

static void handle_dynamic_irq(TrapFrame* tf) {
    const u8 vec = static_cast<u8>(tf->vector);
    const arch::x86_64::interrupts::idt::IrqDesc& desc = arch::x86_64::interrupts::idt::irq_handler_table[vec];
    if (desc.handler) desc.handler(desc.cookie);
    arch::x86_64::interrupts::apic::send_eoi();
}

extern "C" void vespera_trap_handler(TrapFrame* tf) {
    const u8 vec = static_cast<u8>(tf->vector);

    switch (vec) {
        case 0x00:
            divide_error_handler(tf);
            break;
        case 0x06:
            invalid_opcode_handler(tf);
            break;
        case 0x08:
            double_fault_handler(tf);
            break;
        case 0x0B:
            segment_not_present_handler(tf);
            break;
        case 0x0C:
            stack_fault_handler(tf);
            break;
        case 0x0D:
            gp_fault_handler(tf);
            break;
        case 0x0E:
            page_fault_handler(tf);
            break;
        case 0x12:
            machine_check_handler(tf);
            break;

        case 0x21:
            keyboard_int_handler(tf);
            break;
        case 0x22:
            mouse_int_handler(tf);
            break;

        case IRQ_TIMER:
            apic_timer_int_handler(tf);
            break;
        case IRQ_SPURIOUS:
            spurious_int_handler(tf);
            break;
        case IRQ_PANIC:
            panic_ipi_handler(tf);
            break;
        case IRQ_YIELD: {
            u8 cpu_id = cpu_manager::get_current_cpu_id();
            kernel::scheduling::cpu_scheduler::yield_cpu(cpu_id, tf);
            arch::x86_64::interrupts::apic::send_eoi();
            break;
        }

        default:
            if (vec >= arch::x86_64::interrupts::idt::VECTOR_MIN && vec <= arch::x86_64::interrupts::idt::VECTOR_MAX &&
                !arch::x86_64::interrupts::idt::irq_handler_table[vec].free) {
                handle_dynamic_irq(tf);
            } else if (vec >= 0x20) {
                arch::x86_64::interrupts::apic::send_eoi();
            } else {
                unhandled_interrupt_handler(tf);
            }
            break;
    }
}