// ec.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.06.25.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include <acpi/madt.h>

#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "acpi_tables.h"

extern volatile u8* g_local_apic_addr;

namespace kernel::acpi::madt {

    namespace {
        cpu_core g_cpu_cores[MAX_CPU_CORES];
        u32 g_cpu_count = 0;
        u32 g_bsp_apic_id = 0;

        io_apic g_ioapics[MAX_IOAPICS];
        u32 g_ioapic_count = 0;

        interrupt_override g_overrides[MAX_OVERRIDES];
        u32 g_override_count = 0;
    }  // namespace

    void parse(MADT_HEADER* madt) {
        if (madt->flags & 0x1) {
            Log::info("MADT: legacy PIC present");
        }

        const virt_addr_t virt_lapic = phys_to_virt(make_phys(madt->lapic_address));
        kernel::memory::map_memory(virt_lapic, make_phys(madt->lapic_address), (1ULL << PtFlag::CacheDisabled) | (1ULL << PtFlag::ReadWrite));
        g_local_apic_addr = static_cast<volatile u8*>(virt_ptr(virt_lapic));

        auto* entry_ptr = reinterpret_cast<u8*>(madt) + sizeof(MADT_HEADER);
        const auto* end = reinterpret_cast<u8*>(madt) + madt->header.length;

        while (entry_ptr < end) {
            auto* entry_hdr = reinterpret_cast<MADT_ENTRY_HEADER*>(entry_ptr);

            if (entry_hdr->length == 0) {
                Log::error("MADT: entry with length 0 - stopping parse");
                break;
            }

            switch (static_cast<MADT_ENTRY_TYPE>(entry_hdr->type)) {
                case MADT_ENTRY_TYPE::LOCAL_APIC: {
                    auto* e = reinterpret_cast<LOCAL_APIC_ENTRY*>(entry_ptr);
                    if ((e->flags & 0x1) || (e->flags & 0x2)) {
                        if (g_cpu_count < MAX_CPU_CORES) {
                            cpu_core& core = g_cpu_cores[g_cpu_count];
                            core.apic_id = e->apic_id;
                            core.acpi_processor_id = e->acpi_processor_id;
                            core.is_bsp = (g_cpu_count == 0);
                            core.is_online = (e->flags & 0x1) != 0;
                            core.is_enabled = true;

                            if (core.is_bsp) {
                                g_bsp_apic_id = e->apic_id;
                            }
                            g_cpu_count++;
                        }
                    }
                    break;
                }
                case MADT_ENTRY_TYPE::X2_APIC: {
                    auto* e = reinterpret_cast<X2APIC_ENTRY*>(entry_ptr);
                    if ((e->flags & 0x1) || (e->flags & 0x2)) {
                        if (g_cpu_count < MAX_CPU_CORES) {
                            cpu_core& core = g_cpu_cores[g_cpu_count];
                            core.apic_id = e->x2_apic_id;
                            core.acpi_processor_id = e->acpi_id;
                            core.is_bsp = (g_cpu_count == 0);
                            core.is_online = (e->flags & 0x1) != 0;
                            core.is_enabled = true;

                            if (core.is_bsp) {
                                g_bsp_apic_id = e->x2_apic_id;
                            }

                            Log::info(
                                "MADT: X2APIC CPU %u — APIC ID %u, %s, %s",
                                g_cpu_count,
                                e->x2_apic_id,
                                core.is_bsp ? "BSP" : "AP",
                                core.is_online ? "online" : "offline"
                            );
                            g_cpu_count++;
                        }
                    }
                    break;
                }
                case MADT_ENTRY_TYPE::IO_APIC: {
                    auto* e = reinterpret_cast<IOAPIC_ENTRY*>(entry_ptr);
                    if (g_ioapic_count < MAX_IOAPICS) {
                        io_apic& apic = g_ioapics[g_ioapic_count];
                        apic.id = e->ioapic_id;
                        apic.address = static_cast<uptr>(e->ioapic_address);
                        apic.gsi_base = e->gsi_base;
                        g_ioapic_count++;
                    } else {
                        Log::error("MADT: too many IOAPICs (limit %u)", MAX_IOAPICS);
                    }
                    break;
                }
                case MADT_ENTRY_TYPE::INTERRUPT_OVERRIDE: {
                    auto* e = reinterpret_cast<INTERRUPT_OVERRIDE_ENTRY*>(entry_ptr);
                    if (g_override_count < MAX_OVERRIDES) {
                        interrupt_override& ov = g_overrides[g_override_count];
                        ov.bus = e->bus;
                        ov.source_irq = e->irq_source;
                        ov.gsi = e->gsi;
                        ov.flags = e->flags;
                        g_override_count++;
                    }
                    break;
                }
                default:
                    break;
            }

            entry_ptr += entry_hdr->length;
        }

        Log::info("MADT: %u CPU cores, %u IOAPICs, %u IRQ overrides", g_cpu_count, g_ioapic_count, g_override_count);
    }

    u32 cpu_count() {
        return g_cpu_count;
    }
    cpu_core* cpu_cores() {
        return g_cpu_cores;
    }
    u32 bsp_apic_id() {
        return g_bsp_apic_id;
    }
    io_apic* ioapics() {
        return g_ioapics;
    }
    u32 ioapic_count() {
        return g_ioapic_count;
    }
    interrupt_override* overrides() {
        return g_overrides;
    }
    u32 override_count() {
        return g_override_count;
    }

}  // namespace kernel::acpi::madt