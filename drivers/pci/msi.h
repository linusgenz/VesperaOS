// msi.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 28.07.25.
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

#ifndef MSI_H
#define MSI_H

#include "pci.h"

namespace PCI {
    constexpr uint16_t MSI_DELIVERY_MODE_FIXED = (0 << 8);
    constexpr uint32_t MSI_ADDRESS_BASE = 0xFEE00000;

     /*
     * @brief Builds a 32-bit or 64-bit MSI address for xAPIC mode.
     *
     * Usually:
     *   - Bits [31..20] are reserved or fixed.
     *   - Bits [19..12] contain the 8-bit APIC ID for the CPU you want the interrupt on.
     */
    inline uint64_t build_msi_address(uint8_t cpu_apic_id) {
        // Place APIC ID in bits [19..12].
        uint32_t addr_lo = MSI_ADDRESS_BASE | (static_cast<uint32_t>(cpu_apic_id) << 12);
        // For 32-bit addresses, the high part is 0.
        return addr_lo;
    }

    /**
     * @brief MSI capability ID according to the PCI specification.
     */
    constexpr uint8_t MSI_CAPABILITY_ID = 0x05;

    /**
     * @brief Builds the MSI data word, which includes the vector, delivery mode, etc.
     *
     * @param vector The 8-bit interrupt vector you want on that CPU.
     * @param delivery_mode The 3-bit delivery mode (usually 0 = fixed).
     * @return 16-bit message data to write to the MSI capability's message_data.
     */
    inline uint16_t build_msi_data(uint8_t vector, uint16_t delivery_mode = MSI_DELIVERY_MODE_FIXED) {
        uint16_t data = 0;
        data |= (vector & 0xFF); // Bits [7..0] = vector
        data |= delivery_mode; // Bits [10..8] = delivery mode (fixed/lowest/etc.)
        // For a simple scenario, we won't set level or trigger mode bits here.
        return data;
    }

    struct pci_msi_capability {
        union {
            struct {
                uint8_t cap_id;
                uint8_t next_cap_ptr;

                union {
                    struct {
                        uint16_t enable_bit: 1;
                        uint16_t multiple_message_capable: 3;
                        uint16_t multiple_message_enable: 3;
                        uint16_t is_64bit: 1;
                        uint16_t per_vector_masking: 1;
                        uint16_t rsvd0: 7;
                    } __attribute__((packed));

                    uint16_t message_control;
                };
            } __attribute__((packed));

            uint32_t dword0;
        };

        // Message Address (32 or 64 bits)
        union {
            struct {
                uint32_t message_address_lo; // Message Address Lower 32 bits
                uint32_t message_address_hi; // Message Address Upper 32 bits (if 64-bit capable)
            } __attribute__((packed));

            uint64_t message_address; // Full 64-bit Message Address
        };

        uint16_t message_data;
        uint16_t rsvd1;
        uint32_t mask;
        uint32_t pending;
    } __attribute__((packed));

    static_assert(sizeof(pci_msi_capability) == 24);

    bool enable_msi(PCIHeader0* header, uint8_t base_vector, uint8_t wanted = 1);
}
#endif //MSI_H
