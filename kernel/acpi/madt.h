//
// Created by linus on 30.06.25.
//

#ifndef MADT_H
#define MADT_H
#include <vespera/types.h>

#include <acpi/acpi.h>

#define MAX_CPU_CORES 64
#define MAX_IOAPICS 4
#define MAX_OVERRIDES 32

namespace madt {
    struct CpuCore {
        u32 apic_id;
        u32 acpi_processor_id;
        bool is_bsp;  // Bootstrap Processor
        bool is_online;
        bool is_enabled;
    };

    struct IoApic {
        u8 id;
        uptr address;
        u32 gsi_base;
    };

    struct InterruptOverride {
        u8 bus;
        u8 source_irq;
        u32 gsi;
        u16 flags;
    };

    void parse_madt(acpi::MADT_HEADER* madt);
    u32 get_cpu_count();
    CpuCore* get_cpu_cores();
    u32 get_bsp_apic_id();
    IoApic* get_ioapics();
    u32 get_ioapic_count();
    InterruptOverride* get_overrides();
    u32 get_override_count();
}  // namespace MADT
#endif  // MADT_H
