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

#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "msi.h"
#include "pci.h"
#include "pci_bar.h"

namespace pci {

    bool enable_msix(PCI_HEADER0* header, const u8 irq_vector) {
        auto* config_space = reinterpret_cast<u8*>(&header->header);

        if (!(header->header.status & (1u << 4))) {
            Log::error("PCI: No capabilities present");
            return false;
        }

        u8 cap_ptr = header->capabilities_ptr;

        while (cap_ptr) {
            const u8 cap_id = config_space[cap_ptr];
            const u8 next_ptr = config_space[cap_ptr + 1];

            if (cap_id == MSIX_CAPABILITY_ID) {
                volatile auto* msix_cap = reinterpret_cast<volatile PCI_MSIX_CAPABILITY*>(&config_space[cap_ptr]);

                msix_cap->enable_bit = 0;
                msix_cap->function_mask = 1;
                msix_cap->message_control = msix_cap->message_control;

                const u8 table_bar_index = msix_cap->table_bir;
                const u32 table_raw = *reinterpret_cast<volatile u32*>(&config_space[cap_ptr + 4]);
                const u32 table_offset = table_raw & ~0x7u;

                const BarInfo bar_info = pci::bar::read(header, table_bar_index);
                if (!bar_info.is_valid || !bar_info.is_memory) {
                    Log::error("MSI-X: Invalid or non-memory BAR %u", table_bar_index);
                    return false;
                }

                const phys_addr_t bar_phys = make_phys(bar_info.address);
                const u64 bar_size = bar_info.size;

                kernel::memory::map_range(
                    phys_to_virt(bar_phys), bar_phys, bar_size, (1ULL << WriteThrough) | (1ULL << CacheDisabled)
                );

                const virt_addr_t table_base = virt_add(phys_to_virt(bar_phys), table_offset);

                MSIX_TABLE_ENTRY entry;
                entry.message_address = build_msix_address(kernel::interrupts::lapic_get_id());
                entry.message_data = build_msix_data(irq_vector);
                entry.vector_control = 0;

                write_msix_vector_entry(virt_ptr(table_base), 0, entry);

                msix_cap->function_mask = 0;
                msix_cap->enable_bit = 1;
                msix_cap->message_control = msix_cap->message_control;

                return true;
            }

            cap_ptr = next_ptr;
        }

        return false;
    }

    bool try_enable_msi_or_msix(PCI_HEADER0* header, const u8 base_vector, const u8 wanted) {
        if (enable_msix(header, base_vector)) return true;
        return enable_msi(header, base_vector, wanted);
    }
}  // namespace pci