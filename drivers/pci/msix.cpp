// msix.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.07.25.
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

#include "msix.h"
#include "msi.h"
#include "pci.h"
#include "../../include/log.h"
#include "../../arch/x86_64/interrupts/apic.h"

namespace PCI {
#define PCI_BAR_TYPE_32BIT    0x0
#define PCI_BAR_TYPE_64BIT    0x4
#define PCI_BAR_TYPE_MASK     0x6
#define PCI_BAR_MEMORY_MASK   0x1

    /**
     * Prüft ob eine BAR 64-bit ist
     * @param bar_value Der Wert der BAR (z.B. header->BAR0)
     * @return true wenn 64-bit BAR, false wenn 32-bit BAR oder I/O BAR
     */
    bool is_bar_64bit(uint32_t bar_value) {
        // Erstmal prüfen ob es eine Memory BAR ist (Bit 0 = 0)
        if (bar_value & PCI_BAR_MEMORY_MASK) {
            // I/O BAR - niemals 64-bit
            return false;
        }

        // Bits 2:1 prüfen für Memory BAR Type
        uint32_t bar_type = (bar_value >> 1) & 0x3;
        return (bar_type == 0x2); // 0x2 = 64-bit Memory BAR
    }

    struct BarInfo {
        uint64_t address;
        bool is_64bit;
        bool is_memory;
        bool is_prefetchable;
        bool is_valid;
    };

    BarInfo get_bar_info(PCIHeader0 *header, uint8_t bar_index) {
        BarInfo info = {0};

        if (bar_index > 5) {
            return info; // is_valid = false
        }

        uint32_t bar_values[6] = {
            header->BAR0, header->BAR1, header->BAR2,
            header->BAR3, header->BAR4, header->BAR5
        };

        uint32_t bar_value = bar_values[bar_index];

        if (bar_value == 0) {
            return info; // BAR not implemented
        }

        info.is_valid = true;
        info.is_memory = !(bar_value & PCI_BAR_MEMORY_MASK);

        if (!info.is_memory) {
            // I/O BAR
            info.address = bar_value & ~0x3ULL;
            info.is_64bit = false;
            info.is_prefetchable = false;
        } else {
            // Memory BAR
            info.is_64bit = is_bar_64bit(bar_value);
            info.is_prefetchable = (bar_value >> 3) & 1;

            if (info.is_64bit) {
                if (bar_index >= 5) {
                    info.is_valid = false;
                    return info;
                }

                uint32_t bar_high = bar_values[bar_index + 1];
                info.address = ((uint64_t) bar_high << 32) | (bar_value & ~0xFULL);
            } else {
                info.address = bar_value & ~0xFULL;
            }
        }

        return info;
    }

    bool enable_msix(PCIHeader0 *header, uint8_t irq_vector) {
        uint8_t *config_space = reinterpret_cast<uint8_t *>(&header->header);

        if (!(header->header.status & (1 << 4))) {
            Log::Error("PCI: No capabilities present");
            return false;
        }

        uint8_t cap_ptr = header->capabilities_ptr;

        while (cap_ptr) {
            uint8_t cap_id = config_space[cap_ptr];
            uint8_t next_ptr = config_space[cap_ptr + 1];

           /* if (cap_id == PCI::MSIX_CAPABILITY_ID) {
                volatile pci_msix_capability *msix_cap =
                        reinterpret_cast<volatile pci_msix_capability *>(&config_space[cap_ptr]);

                uint32_t table_raw = *reinterpret_cast<volatile uint32_t *>(&config_space[cap_ptr + 4]);
                uint8_t table_bir = table_raw & 0x7;
                uint32_t table_offset = table_raw & ~0x7;

                msix_cap->enable_bit = 0;
                msix_cap->function_mask = 1;
                msix_cap->message_control = msix_cap->message_control; // trigger write


                uint8_t table_bar_index = msix_cap->table_bir;

                BarInfo bar_info = get_bar_info(header, table_bar_index);
                if (!bar_info.is_valid || !bar_info.is_memory) {
                    Log::Error("MSI-X: Invalid or non-memory BAR %u", table_bar_index);
                    return false;
                }

                uint64_t bar_phys = bar_info.address;

                global_page_table_manager.map_range(bar_phys, bar_phys, 0x4000,
                                                    PT_Flag::WriteThrough | PT_Flag::CacheDisabled);


                void *table_base = reinterpret_cast<void *>(bar_phys + table_offset);

                // Entry schreiben
                msix_table_entry entry;
                entry.message_address = build_msix_address(local_apic_get_id());
                entry.message_data = build_msix_data(irq_vector);
                entry.vector_control = 0; // unmasked

                write_msix_vector_entry(table_base, 0, entry);

                // Re-enable MSI-X
                msix_cap->function_mask = 0;
                msix_cap->enable_bit = 1;
                msix_cap->message_control = msix_cap->message_control; // trigger write

                return true;
            }*/

            if (cap_id == PCI::MSI_CAPABILITY_ID) {
                volatile pci_msi_capability *msi_cap =
                    reinterpret_cast<volatile pci_msi_capability *>(&config_space[cap_ptr]);

                uint16_t control = msi_cap->message_control;
                bool is_64_bit = control & (1 << 7);

                msi_cap->message_address = build_msi_address(local_apic_get_id());
                if (is_64_bit) {
                    msi_cap->message_address_hi = 0;
                    msi_cap->message_data = build_msi_data(irq_vector);
                } else {
                    msi_cap->message_data = build_msi_data(irq_vector);
                }

                control |= 1;
                msi_cap->message_control = control;

                Log::Ok("MSI enabled");
                return true;
            }


            cap_ptr = next_ptr;
        }

        Log::Warning("MSI-X capability not found");
        return false;
    }
}
