// fault_logger.cpp
//
// Implementierung des fault-orientierten Debug-Loggers für VesperaOS.
// Nutzt das bestehende Log-System, kapselt aber ein konsistentes,
// auf Faults zugeschnittenes Ausgabeformat.
//
#include "../cpu/cpu_manager.h"
#include "../utils/panic.h"
#if DEBUG_FAULT
#include "trace.h"
#endif
#include <vespera/log.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>

#include "fault_logger.h"
#include "trace.h"

namespace kernel::debug {
    static const char* fault_type_to_string(FaultType type) {
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

    void log_fault(FaultType type, const FaultContext& ctx, const char* extra_msg) {
        const char* type_str = fault_type_to_string(type);

        if (extra_msg && *extra_msg) {
            auto u = scheduling::get_current_unit();
            Log::error(
                "%s: %s on CPU#%u on Unit#%u (%s)",
                type_str,
                extra_msg,
                cpu_manager::get_current_cpu_id(),
                u->id,
                u->name
            );
        } else {
            Log::error("%s", type_str);
        }

        Log::error("  RIP=0x%llx CS=0x%llx RSP=0x%llx RFLAGS=0x%llx", ctx.rip, ctx.cs, ctx.rsp, ctx.rflags);
        u64 fault_addr = 0;
        asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
        Log::error("Page fault address (CR2): %p", fault_addr);
        backtrace(ctx.rbp, ctx.rip);
        panic("FAULT");

        if (ctx.error_code != 0) {
            Log::error("  ERROR_CODE=0x%llx", ctx.error_code);
        }

#if DEBUG_FAULT
        backtrace(ctx.rbp, ctx.rip);
#endif
    }

    void log_page_fault_detail(u64 fault_addr, u64 error_code, const FaultContext& ctx) {
        log_fault(FaultType::PageFault, ctx, "Page fault detected");

        Unit* u = scheduling::get_current_unit();
        Realm* r = RealmManager::get(u->rid);

        Log::error("pml4 kernel: %p current unit pml4: %p", memory::get_pagetable_address(), r->pml4);
        Log::error("  CR2=0x%llx ERROR=0x%llx", fault_addr, error_code);
        Log::error(
            "  Present: %s, Write: %s, User: %s, Reserved: %s",
            (error_code & 1) ? "Yes" : "No",
            (error_code & 2) ? "Yes" : "No",
            (error_code & 4) ? "Yes" : "No",
            (error_code & 8) ? "Yes" : "No"
        );
    }

    void log_invalid_opcode_bytes(u64 rip, const FaultContext& ctx) {
        log_fault(FaultType::InvalidOpcode, ctx, "Invalid opcode detected");

        // Vorsicht: wir greifen direkt auf den Code-Speicher zu – im Fehlerfall ist das
        // ohnehin eine best-effort Debug-Ausgabe.
        const auto opcode_ptr = reinterpret_cast<const u8*>(rip);
        Log::error("  Opcode bytes: %02x %02x %02x %02x", opcode_ptr[0], opcode_ptr[1], opcode_ptr[2], opcode_ptr[3]);
    }
}  // namespace kernel::debug
