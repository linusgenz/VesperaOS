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
#include "pci.h"

namespace pci
{
    bool enable_msi(PCI_HEADER0* header, uint8_t base_vector, uint8_t wanted)
    {
        auto* config_space = reinterpret_cast<uint8_t*>(&header->header);

        if (!(header->header.status & (1 << 4)))
            return false;

        uint8_t cap_ptr = header->capabilities_ptr;

        while (cap_ptr)
        {
            uint8_t cap_id = config_space[cap_ptr];
            uint8_t next_ptr = config_space[cap_ptr + 1];

            if (cap_id == MSI_CAPABILITY_ID)
            {
                volatile auto* msi =
                    reinterpret_cast<volatile PCI_MSI_CAPABILITY*>(&config_space[cap_ptr]);

                uint16_t mc = msi->message_control;

                const bool is64 = mc & (1 << 7);
                const uint8_t mmc = (mc >> 1) & 0b111;

                if (const uint8_t max_vectors = 1 << mmc; wanted > max_vectors)
                    wanted = max_vectors;

                // Translate wanted into encoded MME field
                uint8_t mme = 0;
                while ((1u << mme) < wanted) mme++;

                // Program address
                msi->message_address = build_msi_address(kernel::interrupts::lapic_get_id());
                if (is64)
                    msi->message_address_hi = 0;

                msi->message_data = base_vector;

                // Write MME (multiple message enable)
                mc &= ~((0b111 << 1)); // clear MME bits
                mc |= (mme << 1);

                // Enable MSI
                mc |= 1;

                msi->message_control = mc;
                return true;
            }

            cap_ptr = next_ptr;
        }

        Log::warning("MSI capability not found");
        return false;
    }
}
