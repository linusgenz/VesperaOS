//
// Created by linus on 30.06.25.
//

#include "acpi.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../include/interrupts.h"
#include "../../include/string.h"
#include "../include/basic_renderer.h"
#include "madt.h"

namespace MADT {
    CPUCore cpu_cores[MAX_CPU_CORES];
    volatile uint32_t cpu_count;
    uint32_t bsp_apic_id = 0;

    IoApic ioapics[MAX_IOAPICS];
    uint32_t ioapic_count = 0;

    InterruptOverride overrides[MAX_OVERRIDES];
    uint32_t override_count = 0;

    void parse_madt(ACPI::MADTHeader *madt) {
        bool has_legacy_pic = (madt->flags & 0x1) != 0;
        if (has_legacy_pic) {
            Log::Info("Pic detected");
        } else {
            Log::Info("No Pic detected");
        }

        g_localApicAddr = (uint8_t *) (uintptr_t) madt->lapic_address;

        uint8_t *entries = (uint8_t *) madt + sizeof(ACPI::MADTHeader);
        uint8_t *end = (uint8_t *) madt + madt->header.length;

        while (entries < end) {
            ACPI::MADTEntryHeader *header = (ACPI::MADTEntryHeader *) entries;

            switch ((ACPI::MADTEntryType) header->type) {
                case ACPI::MADTEntryType::LOCAL_APIC: {
                    auto *entry = (ACPI::LocalAPICEntry *) entries;

                    //only available cores
                    if ((entry->flags & 0x1) || (entry->flags & 0x2)) {
                        if (cpu_count < MAX_CPU_CORES) {
                            cpu_cores[cpu_count].apic_id = entry->apic_id;
                            cpu_cores[cpu_count].acpi_processor_id = entry->acpi_processor_id;
                            cpu_cores[cpu_count].is_bsp = (cpu_count == 0); // Erster ist BSP
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
                case ACPI::MADTEntryType::X2APIC: {
                    Log::Info("X2APIC entry detected");
                    auto *entry = (ACPI::X2APICEntry *) entries;

                    if ((entry->flags & 0x1) || (entry->flags & 0x2)) {
                        if (cpu_count < MAX_CPU_CORES) {
                            cpu_cores[cpu_count].apic_id = entry->x2apic_id;
                            cpu_cores[cpu_count].acpi_processor_id = entry->acpi_id;
                            cpu_cores[cpu_count].is_bsp = (cpu_count == 0);
                            cpu_cores[cpu_count].is_online = (entry->flags & 0x1) != 0;
                            cpu_cores[cpu_count].is_enabled = true;

                            if (cpu_cores[cpu_count].is_bsp) {
                                bsp_apic_id = entry->x2apic_id;
                            }

                            Log::Info("CPU %u: X2APIC ID %u, %s, %s",
                                      cpu_count,
                                      entry->x2apic_id,
                                      cpu_cores[cpu_count].is_bsp ? "BSP" : "AP",
                                      cpu_cores[cpu_count].is_online ? "Online" : "Offline");

                            cpu_count++;
                        }
                    }
                    break;
                }
                case ACPI::MADTEntryType::IO_APIC: {
                    auto *entry = (ACPI::IOAPICEntry *) entries;

                    if (ioapic_count < MAX_IOAPICS) {
                        ioapics[ioapic_count].id = entry->ioapic_id;
                        ioapics[ioapic_count].address = (uintptr_t) entry->ioapic_address;
                        ioapics[ioapic_count].gsi_base = entry->gsi_base;
                        ioapic_count++;
                    } else {
                        Log::Error("Too many IOAPICs detected (limit: %u)", MAX_IOAPICS);
                    }
                    break;
                }
                case ACPI::MADTEntryType::INTERRUPT_OVERRIDE: {
                    auto *entry = (ACPI::InterruptOverrideEntry *) entries;

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
                Log::Error("MADT entry with length 0 detected - stopping parse");
                break;
            }


            entries += header->length;
        }

        Log::Info("Detected %u CPU cores", cpu_count);
    }

    uint32_t get_cpu_count() {
        return cpu_count;
    }

    CPUCore *get_cpu_cores() {
        return cpu_cores;
    }

    uint32_t get_bsp_apic_id() {
        return bsp_apic_id;
    }

    IoApic *get_ioapics() {
        return ioapics;
    }

    uint32_t get_ioapic_count() {
        return ioapic_count;
    }

    InterruptOverride *get_overrides() {
        return overrides;
    }

    uint32_t get_override_count() {
        return override_count;
    }
}
