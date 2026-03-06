//
// Created by linus on 30.06.25.
//

#include "madt.h"

#include <kernel/memory.h>
#include <log.h>

#include "../../arch/x86_64/interrupts/apic.h"
#include "acpi.h"

namespace madt {
    CpuCore cpu_cores[MAX_CPU_CORES];
    uint32_t cpu_count;
    uint32_t bsp_apic_id = 0;

    IoApic ioapics[MAX_IOAPICS];
    uint32_t ioapic_count = 0;

    InterruptOverride overrides[MAX_OVERRIDES];
    uint32_t override_count = 0;

    void parse_madt(acpi::MADT_HEADER* madt) {
        if ((madt->flags & 0x1) != 0)  // has legacy pic (support)?
        {
            Log::info("Pic detected");
        } else {
            Log::info("No Pic detected");
        }

        virt_addr_t virt_lapic = phys_to_virt(make_phys(madt->lapic_address));

        kernel::memory::map_memory(
            virt_lapic,
            make_phys(madt->lapic_address),
            (1ULL << PtFlag::CacheDisabled)
        );

        g_local_apic_addr = static_cast<volatile uint8_t*>(virt_ptr(virt_lapic));

        auto* entries = reinterpret_cast<uint8_t*>(madt) + sizeof(acpi::MADT_HEADER);
        const auto* end = reinterpret_cast<uint8_t*>(madt) + madt->header.length;

        while (entries < end) {
            auto* header = reinterpret_cast<acpi::MADT_ENTRY_HEADER*>(entries);

            switch (static_cast<acpi::MADT_ENTRY_TYPE>(header->type)) {
                case acpi::MADT_ENTRY_TYPE::LOCAL_APIC: {
                    // only available cores
                    if (const auto* entry = reinterpret_cast<acpi::LOCAL_APIC_ENTRY*>(entries);
                        (entry->flags & 0x1) || (entry->flags & 0x2)) {
                        if (cpu_count < MAX_CPU_CORES) {
                            cpu_cores[cpu_count].apic_id = entry->apic_id;
                            cpu_cores[cpu_count].acpi_processor_id = entry->acpi_processor_id;
                            cpu_cores[cpu_count].is_bsp = (cpu_count == 0);  // Erster ist BSP
                            cpu_cores[cpu_count].is_online = (entry->flags & 0x1) != 0;
                            cpu_cores[cpu_count].is_enabled = true;

                            if (cpu_cores[cpu_count].is_bsp) {
                                bsp_apic_id = entry->apic_id;
                            }

                            /*      Log::Info("CPU %u: APIC ID %u, %s, %s",
                                           cpu_count,
                                           entry->apic_id,
                                           cpu_cores[cpu_count].is_bsp ? "BSP" : "AP",
                                           cpu_cores[cpu_count].is_online ? "Online" : "Offline");
                                  */
                            cpu_count++;
                        }
                    }
                    break;
                }
                case acpi::MADT_ENTRY_TYPE::X2_APIC: {
                    Log::info("X2APIC entry detected");

                    if (const auto* entry = reinterpret_cast<acpi::X2_APIC_ENTRY*>(entries);
                        (entry->flags & 0x1) || (entry->flags & 0x2)) {
                        if (cpu_count < MAX_CPU_CORES) {
                            cpu_cores[cpu_count].apic_id = entry->x2_apic_id;
                            cpu_cores[cpu_count].acpi_processor_id = entry->acpi_id;
                            cpu_cores[cpu_count].is_bsp = (cpu_count == 0);
                            cpu_cores[cpu_count].is_online = (entry->flags & 0x1) != 0;
                            cpu_cores[cpu_count].is_enabled = true;

                            if (cpu_cores[cpu_count].is_bsp) {
                                bsp_apic_id = entry->x2_apic_id;
                            }

                            Log::info(
                                "CPU %u: X2APIC ID %u, %s, %s",
                                cpu_count,
                                entry->x2_apic_id,
                                cpu_cores[cpu_count].is_bsp ? "BSP" : "AP",
                                cpu_cores[cpu_count].is_online ? "Online" : "Offline"
                            );

                            cpu_count++;
                        }
                    }
                    break;
                }
                case acpi::MADT_ENTRY_TYPE::IO_APIC: {
                    const auto* entry = reinterpret_cast<acpi::IOAPIC_ENTRY*>(entries);

                    if (ioapic_count < MAX_IOAPICS) {
                        ioapics[ioapic_count].id = entry->ioapic_id;
                        ioapics[ioapic_count].address = static_cast<uintptr_t>(entry->ioapic_address);
                        ioapics[ioapic_count].gsi_base = entry->gsi_base;
                        ioapic_count++;
                    } else {
                        Log::error("Too many IOAPICs detected (limit: %u)", MAX_IOAPICS);
                    }
                    break;
                }
                case acpi::MADT_ENTRY_TYPE::INTERRUPT_OVERRIDE: {
                    auto* entry = reinterpret_cast<acpi::InterruptOverrideEntry*>(entries);

                    if (override_count < MAX_OVERRIDES) {
                        overrides[override_count].bus = entry->bus;
                        overrides[override_count].source_irq = entry->irq_source;
                        overrides[override_count].gsi = entry->gsi;
                        overrides[override_count].flags = entry->flags;

                        //     Log::debug("IRQ Override: IRQ %u -> GSI %u (flags 0x%x)",
                        //               entry->irq_source, entry->gsi, entry->flags);

                        override_count++;
                    }
                    break;
                }
                default:
                    break;
            }

            if (header->length == 0) {
                Log::error("MADT entry with length 0 detected - stopping parse");
                break;
            }

            entries += header->length;
        }

        Log::info("Detected %u CPU cores", cpu_count);
    }

    uint32_t get_cpu_count() {
        return cpu_count;
    }

    CpuCore* get_cpu_cores() {
        return cpu_cores;
    }

    uint32_t get_bsp_apic_id() {
        return bsp_apic_id;
    }

    IoApic* get_ioapics() {
        return ioapics;
    }

    uint32_t get_ioapic_count() {
        return ioapic_count;
    }

    InterruptOverride* get_overrides() {
        return overrides;
    }

    uint32_t get_override_count() {
        return override_count;
    }
}  // namespace MADT
