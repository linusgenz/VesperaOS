//
// Created by linus on 30.06.25.
//

#ifndef VESPERAOS_KERNEL_ACPI_MADT_H
#define VESPERAOS_KERNEL_ACPI_MADT_H

#include <vespera/types.h>

#include "../../kernel/acpi/acpi_tables.h"

namespace kernel::acpi::madt {

    constexpr u32 MAX_CPU_CORES = 64;
    constexpr u32 MAX_IOAPICS = 4;
    constexpr u32 MAX_OVERRIDES = 32;

    struct cpu_core {
        u32 apic_id;
        u32 acpi_processor_id;
        bool is_bsp;
        bool is_online;
        bool is_enabled;
    };

    struct io_apic {
        u8 id;
        uptr address;
        u32 gsi_base;
    };

    struct interrupt_override {
        u8 bus;
        u8 source_irq;
        u32 gsi;
        u16 flags;
    };

    void parse(MADT_HEADER* madt);

    [[nodiscard]] u32 cpu_count();
    [[nodiscard]] cpu_core* cpu_cores();
    [[nodiscard]] u32 bsp_apic_id();
    [[nodiscard]] io_apic* ioapics();
    [[nodiscard]] u32 ioapic_count();
    [[nodiscard]] interrupt_override* overrides();
    [[nodiscard]] u32 override_count();

}  // namespace kernel::acpi::madt

#endif  // VESPERAOS_KERNEL_ACPI_MADT_H
