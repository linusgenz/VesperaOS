// fault_logger.cpp
//
// Implementierung des fault-orientierten Debug-Loggers für VesperaOS.
// Nutzt das bestehende Log-System, kapselt aber ein konsistentes,
// auf Faults zugeschnittenes Ausgabeformat.
//
#include "../cpu/cpu_manager.h"
#include "../realm/address_space.h"
#include "../utils/panic.h"
#if DEBUG_FAULT
#include "trace.h"
#endif
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <kernel/units/unit.h>

#include <kernel/scheduling/scheduler_types.h>
#include "fault_logger.h"
#include "trace.h"

namespace kernel::debug {
    static const char* fault_type_to_string(const FaultType type) {
        switch (type) {
            case FaultType::PageFault:
                return "PAGE FAULT";
            case FaultType::DoubleFault:
                return "DOUBLE FAULT";
            case FaultType::GeneralProtection:
                return "GENERAL PROTECTION FAULT";
            case FaultType::StackFault:
                return "STACK FAULT";
            case FaultType::SegmentNotPresent:
                return "SEGMENT NOT PRESENT";
            case FaultType::DivideByZero:
                return "DIVIDE BY ZERO";
            case FaultType::MachineCheck:
                return "MACHINE CHECK EXCEPTION";
            case FaultType::UnhandledInterrupt:
                return "UNHANDLED INTERRUPT";
            case FaultType::InvalidOpcode:
                return "INVALID OPCODE";
            case FaultType::Unknown:
            default:
                return "UNKNOWN FAULT";
        }
    }

    void log_fault(FaultType type, const TrapFrame* ctx, const char* extra_msg) {
        const char* type_str = fault_type_to_string(type);

        if (extra_msg && *extra_msg) {
            auto u = scheduling::get_current_unit();
            Log::error(
                "%s: %s on CPU#%u on Unit#%u (%s) (%s)",
                type_str,
                extra_msg,
                cpu_manager::get_current_cpu_id(),
                u->id,
                u->name,
                u->parent->name
            );
        } else {
            Log::error("%s", type_str);
        }

        Log::error("  RIP=0x%llx CS=0x%llx RSP=0x%llx RFLAGS=0x%llx", ctx->rip, ctx->cs, ctx->rsp, ctx->rflags);
        u64 fault_addr = 0;
        asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
        Log::error("pml4 kernel: %p", memory::get_pagetable_address());
        Log::error("  CR2=0x%llx ERROR=0x%llx", fault_addr, ctx->error_code);
        Log::error("rax: 0x%llx rbx: 0x%llx rcx: 0x%llx, rdx: 0x%llx rsi: 0x%llx, rdi: 0x%llx rbp: 0x%llx r8: 0x%llx", ctx->rax, ctx->rbx, ctx->rcx, ctx->rdx, ctx->rsi, ctx->rdi, ctx->rbp, ctx->r8);
        backtrace(ctx->rbp, ctx->rip);

        scheduling::cpu_scheduler::CpuScheduler* cpu = scheduling::get_cpu_data(6);

        auto print_unit_backtrace = [](const Unit* u) {
            if (!u || u->state == UnitState::Terminated) return;
            Log::error("=== Unit#%u (%s) state=%u ===", u->id, u->name, (u8)u->state);
            backtrace(
                u->context.cpu_ctx.rbp,
                u->context.cpu_ctx.rip
            );
        };

        cpu->ready_queue.for_each(print_unit_backtrace);
        cpu->blocked_queue.for_each(print_unit_backtrace);

        panic("FAULT");

        if (ctx->error_code != 0) {
            Log::error("  ERROR_CODE=0x%llx", ctx->error_code);
        }

#if DEBUG_FAULT
        backtrace(ctx->rbp, ctx->rip);
#endif
    }

    void log_page_fault_detail(const u64 fault_addr, const u64 error_code, const TrapFrame* ctx) {
        log_fault(FaultType::PageFault, ctx, "Page fault detected");

        const Unit* u = scheduling::get_current_unit();
        const Realm* r = u->parent;

        Log::error("pml4 kernel: %p current unit pml4: %p", memory::get_pagetable_address(), r->address_space->pml4_phys());
        Log::error("  CR2=0x%llx ERROR=0x%llx", fault_addr, error_code);
        Log::error(
            "  Present: %s, Write: %s, User: %s, Reserved: %s",
            (error_code & 1) ? "Yes" : "No",
            (error_code & 2) ? "Yes" : "No",
            (error_code & 4) ? "Yes" : "No",
            (error_code & 8) ? "Yes" : "No"
        );
    }

    void log_invalid_opcode_bytes(const u64 rip, const TrapFrame* ctx) {
        log_fault(FaultType::InvalidOpcode, ctx, "Invalid opcode detected");

        // Vorsicht: wir greifen direkt auf den Code-Speicher zu – im Fehlerfall ist das
        // ohnehin eine best-effort Debug-Ausgabe.
        const auto opcode_ptr = reinterpret_cast<const u8*>(rip);
        Log::error("  Opcode bytes: %02x %02x %02x %02x", opcode_ptr[0], opcode_ptr[1], opcode_ptr[2], opcode_ptr[3]);
    }
}  // namespace kernel::debug
