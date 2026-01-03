// fault_logger.cpp
//
// Implementierung des fault-orientierten Debug-Loggers für VesperaOS.
// Nutzt das bestehende Log-System, kapselt aber ein konsistentes,
// auf Faults zugeschnittenes Ausgabeformat.
//
#if DEBUG_FAULT
#include "trace.h"
#endif

#include "fault_logger.h"
#include <kernel/scheduling.h>
#include "../../include/log.h"
#include "../cpu/cpu_manager.h"
#include <kernel/realm/realm_manager.h>

namespace kernel::debug
{
    static const char* fault_type_to_string(FaultType type)
    {
        switch (type)
        {
        case FaultType::PageFault: return "PAGE FAULT";
        case FaultType::DoubleFault: return "DOUBLE FAULT";
        case FaultType::GeneralProtection: return "GENERAL PROTECTION FAULT";
        case FaultType::StackFault: return "STACK FAULT";
        case FaultType::SegmentNotPresent: return "SEGMENT NOT PRESENT";
        case FaultType::DivideByZero: return "DIVIDE BY ZERO";
        case FaultType::MachineCheck: return "MACHINE CHECK EXCEPTION";
        case FaultType::UnhandledInterrupt: return "UNHANDLED INTERRUPT";
        case FaultType::InvalidOpcode: return "INVALID OPCODE";
        case FaultType::Unknown:
        default: return "UNKNOWN FAULT";
        }
    }

    void log_fault(FaultType type, const FaultContext& ctx, const char* extra_msg)
    {
        const char* type_str = fault_type_to_string(type);

        if (extra_msg && *extra_msg)
        {
            auto u = scheduling::get_current_unit();
            Log::Error("%s: %s on CPU#%u on Unit#%u (%s)", type_str, extra_msg, CPUManager::get_current_cpu_id(), u->id,
                       u->name);
        }
        else
        {
            Log::Error("%s", type_str);
        }

        Log::Error("  RIP=0x%llx CS=0x%llx RSP=0x%llx RFLAGS=0x%llx",
                   ctx.rip, ctx.cs, ctx.rsp, ctx.rflags);

        if (ctx.error_code != 0)
        {
            Log::Error("  ERROR_CODE=0x%llx", ctx.error_code);
        }

#if DEBUG_FAULT
        backtrace(ctx.rbp, ctx.rip);
#endif
    }

    void log_page_fault_detail(uint64_t fault_addr, uint64_t error_code, const FaultContext& ctx)
    {
        log_fault(FaultType::PageFault, ctx, "Page fault detected");

        Unit* u = scheduling::get_current_unit();
        Realm* r = RealmManager::get(u->rid);

        Log::Error("pml4 kernel: %p current unit pml4: %p", memory::get_pagetable_address(), r->pml4);
        Log::Error("  CR2=0x%llx ERROR=0x%llx", fault_addr, error_code);
        Log::Error("  Present: %s, Write: %s, User: %s, Reserved: %s",
                   (error_code & 1) ? "Yes" : "No",
                   (error_code & 2) ? "Yes" : "No",
                   (error_code & 4) ? "Yes" : "No",
                   (error_code & 8) ? "Yes" : "No");
    }

    void log_invalid_opcode_bytes(uint64_t rip, const FaultContext& ctx)
    {
        log_fault(FaultType::InvalidOpcode, ctx, "Invalid opcode detected");

        // Vorsicht: wir greifen direkt auf den Code-Speicher zu – im Fehlerfall ist das
        // ohnehin eine best-effort Debug-Ausgabe.
        const auto opcode_ptr = reinterpret_cast<const uint8_t*>(rip);
        Log::Error("  Opcode bytes: %02x %02x %02x %02x",
                   opcode_ptr[0], opcode_ptr[1], opcode_ptr[2], opcode_ptr[3]);
    }
} // namespace kernel::debug
