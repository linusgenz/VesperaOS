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

namespace pci {
    constexpr u16 MSI_DELIVERY_MODE_FIXED = (0 << 8);
    constexpr u32 MSI_ADDRESS_BASE = 0xFEE00000;

     /*
     * @brief Builds a 32-bit or 64-bit MSI address for xAPIC mode.
     *
     * Usually:
     *   - Bits [31..20] are reserved or fixed.
     *   - Bits [19..12] contain the 8-bit APIC ID for the CPU you want the interrupt on.
     */
    inline u64 build_msi_address(u8 cpu_apic_id) {
        // Place APIC ID in bits [19..12].
        u32 addr_lo = MSI_ADDRESS_BASE | (static_cast<u32>(cpu_apic_id) << 12);
        // For 32-bit addresses, the high part is 0.
        return addr_lo;
    }

    /**
     * @brief MSI capability ID according to the PCI specification.
     */
    constexpr u8 MSI_CAPABILITY_ID = 0x05;

    /**
     * @brief Builds the MSI data word, which includes the vector, delivery mode, etc.
     *
     * @param vector The 8-bit interrupt vector you want on that CPU.
     * @param delivery_mode The 3-bit delivery mode (usually 0 = fixed).
     * @return 16-bit message data to write to the MSI capability's message_data.
     */
    inline u16 build_msi_data(u8 vector, u16 delivery_mode = MSI_DELIVERY_MODE_FIXED) {
        u16 data = 0;
        data |= (vector & 0xFF); // Bits [7..0] = vector
        data |= delivery_mode; // Bits [10..8] = delivery mode (fixed/lowest/etc.)
        // For a simple scenario, we won't set level or trigger mode bits here.
        return data;
    }

    struct PCI_MSI_CAPABILITY {
        union {
            struct {
                u8 cap_id;
                u8 next_cap_ptr;

                union {
                    struct {
                        u16 enable_bit: 1;
                        u16 multiple_message_capable: 3;
                        u16 multiple_message_enable: 3;
                        u16 is_64_bit: 1;
                        u16 per_vector_masking: 1;
                        u16 rsvd0: 7;
                    } __attribute__((packed));

                    u16 message_control;
                };
            } __attribute__((packed));

            u32 dword0;
        };

        // Message Address (32 or 64 bits)
        union {
            struct {
                u32 message_address_lo; // Message Address Lower 32 bits
                u32 message_address_hi; // Message Address Upper 32 bits (if 64-bit capable)
            } __attribute__((packed));

            u64 message_address; // Full 64-bit Message Address
        };

        u16 message_data;
        u16 rsvd1;
        u32 mask;
        u32 pending;
    } __attribute__((packed));

    static_assert(sizeof(PCI_MSI_CAPABILITY) == 24);

    bool enable_msi(PCI_HEADER0* header, u8 base_vector, u8 wanted = 1);
}
#endif //MSI_H
