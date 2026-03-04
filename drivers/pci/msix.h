// msi.h
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

#ifndef MSIX_H
#define MSIX_H

#include "pci.h"
#include <cstddef>

namespace PCI {
    /**
     * @brief Base address for local APIC in MSI-X message address (similar to MSI).
     *
     * For xAPIC mode, MSI-X interrupts are delivered to 0xFEE0'0000 (same as MSI).
     * In x2APIC mode, a different scheme may be needed.
     */
    constexpr uint32_t MSIX_ADDRESS_BASE = 0xFEE00000;

    /**
     * @brief MSI-X capability ID according to the PCI specification.
     */
    constexpr uint8_t MSIX_CAPABILITY_ID = 0x11;

    /**
     * @brief Offsets and fields in the MSI-X control register.
     */
    constexpr uint16_t MSIX_CONTROL_ENABLE_BIT = (1 << 15);  // MSI-X Enable bit in control register
    constexpr uint16_t MSIX_MASK_ALL_VECTORS   = (1 << 14);  // Mask all MSI-X vectors

    /**
     * @brief Offsets/bits in the MSI-X data (message_data).
     *
     * Bits 7..0:   Interrupt Vector
     * Bits 10..8:  Delivery Mode
     * Bit  14:     Level
     * Bit  15:     Trigger Mode
     * etc.
     */
    constexpr uint16_t MSIX_DELIVERY_MODE_FIXED  = (0 << 8);
    constexpr uint16_t MSIX_DELIVERY_MODE_LOWEST = (1 << 8);
    constexpr uint16_t MSIX_DELIVERY_MODE_NMI    = (4 << 8);

    /**
     * @brief Structure for an entry in the MSI-X vector table.
     */
    struct msix_table_entry {
        uint64_t message_address;   // MSI-X message address
        uint32_t message_data;      // MSI-X message data
        uint32_t vector_control;    // Vector control (bit 0 is the mask bit)

        /**
         * @brief Masks this vector by setting the mask bit.
         */
        void mask() {
            vector_control |= 1;
        }

        /**
         * @brief Unmasks this vector by clearing the mask bit.
         */
        void unmask() {
            vector_control &= ~1;
        }
    } __attribute__((packed));

    static_assert(sizeof(msix_table_entry) == 16, "msix_table_entry must be 16 bytes");

    /**
     * @brief MSI-X capability structure from the PCI configuration space.
     *
     * This is read during MSI-X initialization to determine the location
     * of the vector table and pending bit array (PBA).
     */
    struct pci_msix_capability {
        union {
            struct {
                uint8_t cap_id;
                uint8_t next_cap_ptr;
                union {
                    struct {
                        // Table Size is N - 1 encoded, and is the number of
                        // entries in the MSI-X table. This field is Read-Only.
                        uint16_t table_size      : 11;
                        uint16_t rsvd0           : 3;
                        uint16_t function_mask   : 1;
                        uint16_t enable_bit      : 1;
                    } __attribute__((packed));
                    uint16_t message_control;
                } __attribute__((packed));
            } __attribute__((packed));
            uint32_t dword0;
        };

        union {
            struct {
                // BIR specifies which BAR is used for the Message Table. This may be a 64-bit
                // BAR, and is zero-indexed (so BIR=0, BAR0, offset 0x10 into the header).
                uint32_t table_bir       : 3;
                uint32_t table_offset    : 29;
            } __attribute__((packed));
            uint32_t dword1;
        };

        union {
            struct {
                // BIR specifies which BAR is used for the Message Table. This may be a 64-bit
                // BAR, and is zero-indexed (so BIR=0, BAR0, offset 0x10 into the header).
                uint32_t pba_bir       : 3;
                uint32_t pba_offset    : 29;
            } __attribute__((packed));
            uint32_t dword2;
        };
    } __attribute__((packed));
    static_assert(sizeof(pci_msix_capability) == 12, "pci_msix_capability must be 12 bytes");

    /**
     * @brief Builds a 32-bit or 64-bit MSI-X message address for xAPIC mode.
     *
     * Similar to MSI, this function encodes the target CPU's APIC ID into
     * the MSI-X message address.
     *
     * @param cpu_apic_id The target CPU's APIC ID.
     * @return The 64-bit message address to be written to the vector table.
     */
    inline uint64_t build_msix_address(uint8_t cpu_apic_id) {
        // Same as MSI: place the APIC ID in bits [19..12].
        uint32_t addr_lo = MSIX_ADDRESS_BASE | (static_cast<uint32_t>(cpu_apic_id) << 12);
        return addr_lo;
    }

    /**
     * @brief Builds the MSI-X data word, which includes the vector and delivery mode.
     *
     * The delivery mode is typically "fixed" for MSI-X interrupts. Additional options
     * such as level/trigger configuration can be added as needed.
     *
     * @param vector The 8-bit interrupt vector.
     * @param delivery_mode The delivery mode (typically "fixed").
     * @return The 16-bit message data for the MSI-X vector table.
     */
    inline uint16_t build_msix_data(uint8_t vector, uint16_t delivery_mode = MSIX_DELIVERY_MODE_FIXED) {
        uint16_t data = vector & 0xFF;  // Bits [7..0] = vector
        data |= delivery_mode;          // Bits [10..8] = delivery mode
        return data;
    }

    /**
     * @brief Reads an entry from the MSI-X vector table.
     *
     * This function reads the specified MSI-X vector table entry and returns a copy of it.
     *
     * @param base_address The base address of the MSI-X vector table (mapped memory).
     * @param vector_index The index of the vector to read.
     * @return A copy of the msix_table_entry for the specified vector.
     */
    inline msix_table_entry read_msix_vector_entry(void* base_address, size_t vector_index) {
        uint8_t* entry_addr = static_cast<uint8_t*>(base_address) + (vector_index * sizeof(msix_table_entry));

        volatile auto* data = reinterpret_cast<volatile uint32_t*>(entry_addr);

        msix_table_entry result{};
        result.message_address = static_cast<uint64_t>(data[1]) << 32 | data[0];
        result.message_data    = data[2];
        result.vector_control  = data[3];
        return result;
    }

    /**
     * @brief Writes an MSI-X vector table entry.
     *
     * Updates the specified MSI-X vector table entry with the provided values.
     *
     * @param base_address The base address of the MSI-X vector table (mapped memory).
     * @param vector_index The index of the vector to write.
     * @param entry The msix_table_entry to write to the table.
     */
    inline void write_msix_vector_entry(void* base_address, size_t vector_index, const msix_table_entry& entry) {
        volatile auto* entry_addr = reinterpret_cast<volatile uint32_t*>(
            static_cast<uint8_t*>(base_address) + vector_index * sizeof(msix_table_entry)
        );

        entry_addr[0] = static_cast<uint32_t>(entry.message_address);          // Address Low
        entry_addr[1] = static_cast<uint32_t>(entry.message_address >> 32);    // Address High
        entry_addr[2] = entry.message_data;
        entry_addr[3] = entry.vector_control;

        __asm__ volatile("sfence" ::: "memory");
    }

    /**
     * @brief Clears the pending bit for a given MSI-X vector.
     *
     * The Pending Bit Array (PBA) keeps track of pending MSI-X interrupts. This function
     * clears the pending bit to acknowledge the interrupt.
     *
     * @param pba_base The base address of the PBA (mapped memory).
     * @param vector_index The index of the vector to clear.
     */
    inline void clear_msix_pending_bit(void* pba_base, size_t vector_index) {
        size_t byte_offset = (vector_index / 8);
        size_t bit_offset = (vector_index % 8);
        uint8_t* byte_ptr = static_cast<uint8_t*>(pba_base) + byte_offset;

        // Clear the corresponding bit
        *byte_ptr &= ~(1 << bit_offset);
    }

    bool try_enable_msi_or_msix(PCIHeader0* header, uint8_t base_vector, uint8_t wanted = 0);
}
#endif //MSIX_H
