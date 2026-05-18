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

#include <vespera/interrupts.h>
#include <vespera/log.h>

#include "../../arch/x86_64/interrupts/apic.h"
#include "pci.h"

namespace pci {
    bool enable_msi(volatile PCI_HEADER0* header, const u8 base_vector, u8 wanted) {
        auto* config_space = reinterpret_cast<volatile u8*>(&header->header);

        if (!(header->header.status & (1 << 4))) return false;

        u8 cap_ptr = header->capabilities_ptr;

        while (cap_ptr) {
            const u8 cap_id = config_space[cap_ptr];
            const u8 next_ptr = config_space[cap_ptr + 1];

            if (cap_id == MSI_CAPABILITY_ID) {
                auto* cap = reinterpret_cast<volatile PCI_MSI_CAP_HEADER*>(&config_space[cap_ptr]);

                u16 mc = cap->message_control;

                const bool is64 = mc & (1 << 7);
                const u8 mmc = (mc >> 1) & 0b111;

                if (const u8 max_vectors = 1 << mmc; wanted > max_vectors) wanted = max_vectors;

                // Translate wanted into encoded MME field
                u8 mme = 0;
                while ((1u << mme) < wanted) mme++;

                // Program address
                const u64 addr = build_msi_address(arch::x86_64::interrupts::apic::get_id());

                if (is64) {
                    auto* msi = reinterpret_cast<volatile PCI_MSI_CAPABILITY_64*>(cap);
                    msi->message_address_lo = static_cast<u32>(addr & 0xFFFFFFFF);
                    msi->message_address_hi = static_cast<u32>(addr >> 32);
                    msi->message_data = base_vector;
                    msi->header.message_control |= 1;
                } else {
                    auto* msi = reinterpret_cast<volatile PCI_MSI_CAPABILITY_32*>(cap);
                    msi->message_address = static_cast<u32>(addr & 0xFFFFFFFF);
                    msi->message_data = base_vector;
                    msi->header.message_control |= 1;
                }

                // Write MME (multiple message enable)
                mc &= ~((0b111 << 1));  // clear MME bits
                mc |= (mme << 1);

                // Enable MSI
                mc |= 1;

                cap->message_control = mc;
                return true;
            }

            cap_ptr = next_ptr;
        }

        Log::warning("MSI capability not found");
        return false;
    }
}  // namespace pci
