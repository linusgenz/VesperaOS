// msi.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.07.25.
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

#include "msi.h"
#include "../../include/log.h"
#include "../../arch/x86_64/interrupts/apic.h"
#include "../../kernel/include/interrupts.h"
#include "pci.h"

namespace PCI {

    bool enable_msi(PCIHeader0* header, uint8_t irq_vector) {
        uint8_t* config_space = reinterpret_cast<uint8_t*>(&header->header);

        if (!(header->header.status & (1 << 4))) {
            Log::Error("PCI: No capabilities present");
            return false;
        }

        uint8_t cap_ptr = header->capabilities_ptr;

        while (cap_ptr) {
            uint8_t cap_id = config_space[cap_ptr];
            uint8_t next_ptr = config_space[cap_ptr + 1];

            if (cap_id == PCI::MSI_CAPABILITY_ID) {
                volatile pci_msi_capability* msi_cap =
                    reinterpret_cast<volatile pci_msi_capability*>(&config_space[cap_ptr]);

                uint16_t control = msi_cap->message_control;
                bool is_64_bit = control & (1 << 7);

                msi_cap->message_address = build_msi_address(kernel::interrupts::lapic_get_id());

                if (is_64_bit) {
                    msi_cap->message_address_hi = 0;
                }

                msi_cap->message_data = build_msi_data(irq_vector);
                msi_cap->message_control = control | 1;

                Log::Ok("MSI enabled");
                return true;
            }

            cap_ptr = next_ptr;
        }

        Log::Warning("MSI capability not found");
        return false;
    }
}