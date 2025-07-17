//
// Created by linus on 30.06.25.
//

#include "acpi.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../include/log.h"
#include "../../arch/x86_64/interrupts/interrupts.h"
#include "../../include/string.h"
#include "../include/basic_renderer.h"
#include "madt.h"

namespace MADT {
    // Globale Arrays für CPU-Kern-Verwaltung
    CPUCore cpu_cores[MAX_CPU_CORES];
    volatile uint32_t cpu_count;
    uint32_t bsp_apic_id = 0;
    
    void parse_madt(ACPI::MADTHeader* madt) {
        bool has_legacy_pic = (madt->flags  & 0x1) != 0;
        if (has_legacy_pic) {
            Log::Info("Pic detected");
        } else {
            Log::Info("No Pic detected");
        }

        g_localApicAddr = (uint8_t *)(uintptr_t)madt->lapic_address;
        Log::Info("LAPIC Address: 0x%llx", g_localApicAddr);
        *(volatile uint64_t*)0x6000 = (uint64_t)madt->lapic_address;

        uint8_t* entries = (uint8_t*)madt + sizeof(ACPI::MADTHeader);
        uint8_t* end = (uint8_t*)madt + madt->header.length;

        while (entries < end) {
            ACPI::MADTEntryHeader* header = (ACPI::MADTEntryHeader*)entries;

            switch ((ACPI::MADTEntryType)header->type) {
                case ACPI::MADTEntryType::LOCAL_APIC: {
                    auto* entry = (ACPI::LocalAPICEntry*)entries;

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
                            
                            Log::Info("CPU %u: APIC ID %u, %s, %s", 
                                     cpu_count, 
                                     entry->apic_id,
                                     cpu_cores[cpu_count].is_bsp ? "BSP" : "AP",
                                     cpu_cores[cpu_count].is_online ? "Online" : "Offline");
                            
                            cpu_count++;
                        }
                    }
                    break;
                }
                case ACPI::MADTEntryType::X2APIC: {
                    Log::Info("X2APIC entry detected");
                    auto* entry = (ACPI::X2APICEntry*)entries;
                    
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
                    auto* entry = (ACPI::IOAPICEntry*)entries;
                    // IO APIC Adresse, GSI base etc. speichern
                    break;
                }
                case ACPI::MADTEntryType::INTERRUPT_OVERRIDE: {
                    auto* entry = (ACPI::InterruptOverrideEntry*)entries;
                    // IRQ → GSI mapping merken
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
    
    CPUCore* get_cpu_cores() {
        return cpu_cores;
    }
    
    uint32_t get_bsp_apic_id() {
        return bsp_apic_id;
    }

}
