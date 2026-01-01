//
// Created by linus on 30.06.25.
//

#ifndef MADT_H
#define MADT_H
#include "acpi.h"
#include <cstdint>

#define MAX_CPU_CORES 64
#define MAX_IOAPICS 4
#define MAX_OVERRIDES 32

namespace MADT {
    struct CPUCore {
        uint32_t apic_id;
        uint32_t acpi_processor_id;
        bool is_bsp;        // Bootstrap Processor
        bool is_online;
        bool is_enabled;
    };

    struct IoApic {
        uint8_t id;
        uintptr_t address;
        uint32_t gsi_base;
    };

    struct InterruptOverride {
        uint8_t bus;
        uint8_t source_irq;
        uint32_t gsi;
        uint16_t flags;
    };
    
    void parse_madt(ACPI::MADTHeader* madt);
    uint32_t get_cpu_count();
    CPUCore* get_cpu_cores();
    uint32_t get_bsp_apic_id();
    IoApic* get_ioapics();
    uint32_t get_ioapic_count();
    InterruptOverride* get_overrides();
    uint32_t get_override_count();
}
#endif //MADT_H
