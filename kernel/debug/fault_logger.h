// fault_logger.h
//
// VesperaOS - fault-oriented debug logger for CPU exceptions
//
// Designed to be safe to call from fault/interrupt context:
//  * keine dynamischen Allokationen
//  * nur primitive Typen als Input
//  * nur best-effort Logging über das bestehende Log-System
//
// Copyright (c) 2025 Linus Genz
//
#ifndef VESPERAOS_FAULT_LOGGER_H
#define VESPERAOS_FAULT_LOGGER_H

#include <vespera/types.h>

namespace kernel::debug {

    enum class FaultType : u8 {
        PageFault,
        DoubleFault,
        GeneralProtection,
        StackFault,
        SegmentNotPresent,
        DivideByZero,
        MachineCheck,
        UnhandledInterrupt,
        InvalidOpcode,
        Unknown
    };

    struct FaultContext {
        u64 rip;
        u64 cs;
        u64 rsp;
        u64 rflags;
        u64 rbp;
        u64 error_code;
    };

    void log_fault(FaultType type, const FaultContext& ctx, const char* extra_msg = nullptr);

    void log_page_fault_detail(u64 fault_addr, u64 error_code, const FaultContext& ctx);

    void log_invalid_opcode_bytes(u64 rip, const FaultContext& ctx);

}  // namespace kernel::debug

#endif  // VESPERAOS_FAULT_LOGGER_H