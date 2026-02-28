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

#include <cstdint>

namespace kernel::debug {

    enum class FaultType : uint8_t {
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
        uint64_t rip;
        uint64_t cs;
        uint64_t rsp;
        uint64_t rflags;
        uint64_t rbp;
        uint64_t error_code;
    };

    void log_fault(FaultType type, const FaultContext& ctx, const char* extra_msg = nullptr);

    void log_page_fault_detail(uint64_t fault_addr, uint64_t error_code, const FaultContext& ctx);

    void log_invalid_opcode_bytes(uint64_t rip, const FaultContext& ctx);

}  // namespace kernel::debug

#endif  // VESPERAOS_FAULT_LOGGER_H