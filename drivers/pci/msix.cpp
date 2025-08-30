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
#include "../../kernel/include/interrupts.h"

namespace PCI {

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

            if (cap_id == PCI::MSIX_CAPABILITY_ID) {
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

                kernel::memory::map_range((void*)bar_phys, (void*)bar_phys, 0x4000,
                                                    (1ULL << PT_Flag::WriteThrough) | (1ULL << PT_Flag::CacheDisabled));


                void *table_base = reinterpret_cast<void *>(bar_phys + table_offset);

                // Entry schreiben
                msix_table_entry entry;
                entry.message_address = build_msix_address(kernel::interrupts::lapic_get_id());
                entry.message_data = build_msix_data(irq_vector);
                entry.vector_control = 0; // unmasked

                write_msix_vector_entry(table_base, 0, entry);

                // Re-enable MSI-X
                msix_cap->function_mask = 0;
                msix_cap->enable_bit = 1;
                msix_cap->message_control = msix_cap->message_control; // trigger write

                Log::debug("MSIX enabled");
                return true;
            }

            cap_ptr = next_ptr;
        }

        Log::Warning("MSI-X capability not found");
        return false;
    }

    bool try_enable_msi_or_msix(PCIHeader0* header, uint8_t irq_vector) {
        if (enable_msix(header, irq_vector)) return true;
        return enable_msi(header, irq_vector);
    }
}