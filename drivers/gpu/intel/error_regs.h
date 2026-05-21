// error_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.05.26.
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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS.  If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_ERROR_REGS_H
#define VESPERAOS_ERROR_REGS_H

// IHD-OS-KBL-Vol 2c — EMR, ESR, EIR (Error Mask / Status / Identity Registers)

// ============================================================================
// EMR — Error Mask Register
// ============================================================================

/**
 * @brief Error Mask Register (EMR).
 *
 * Controls which ESR error bits are propagated into EIR and subsequently
 * into the Master Error condition of ISR.
 *
 * @note 0 = unmasked — error propagates into EIR.
 * @note 1 = masked   — error suppressed (default; reset value = 0xFFFFFFFF).
 * @note Bits [31:8] are reserved, RO in hardware, and MUST be written as 1.
 * @note EMR only gates EIR propagation; it does NOT prevent the ESR bit
 *       from being set by hardware.
 */
union EMR_REG {
    struct {
        /**
         * @brief Error mask bits [7:0].
         *
         * One bit per ESR error source.
         * Bit meanings are engine-specific; see the Hardware-Detected Error
         * Bits table for BCS.
         *
         * 0 = error reported into EIR
         * 1 = error masked
         */
        u32 error_mask : 8;  ///< [7:0]

        /**
         * @brief Reserved [31:8].
         *
         * Not implemented in hardware.  MUST be written as 1.
         */
        u32 reserved : 24;  ///< [31:8]  write-as-1
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(EMR_REG) == 4);

// ============================================================================
// EIR — Error Identity Register
// ============================================================================

/**
 * @brief BCS-specific error bits within EIR [15:0].
 */
struct BCS_EIR_BITS {
    /**
     * @brief Instruction Error [0].
     *
     * Fatal; set on:
     *   - unsupported client ID
     *   - invalid instruction decode
     *   - deprecated MI opcode
     *
     * @note Cannot be cleared by software; requires hardware reset.
     */
    u16 instruction_error : 1;  ///< [0]

    u16 reserved1 : 1;  ///< [1]  MBZ

    /**
     * @brief Command Privilege Violation [2].
     *
     * A privileged command was issued from a non-privileged batch buffer.
     * Hardware converts the command to a NOOP and continues parsing.
     */
    u16 privilege_violation : 1;  ///< [2]

    u16 reserved3_15 : 13;  ///< [15:3] MBZ
} __attribute__((packed));

static_assert(sizeof(BCS_EIR_BITS) == 2);

/**
 * @brief Error Identity Register (EIR).
 *
 * Holds persistent error status for conditions unmasked via EMR.
 * Any set bit in [15:0] drives the Master Error bit in ISR.
 *
 * Clear sequence:
 *   1. Write 1 to the relevant bit(s) in [15:0] to clear the error.
 *   2. If needed, clear master_error in IIR afterward.
 *
 * @note Bits [15:0] are W1C.
 * @note Bit 0 (Instruction Error) and Bit 4 (Page Table Error) are fatal —
 *       cannot be cleared except by reset.
 * @note Bits [31:16] are WO mask field; reserved bits are RO.
 * @note Default reset value = 0x00000000.
 *
 * MMIO: 0x220B0 (EIR_BCSUNIT)
 */
union EIR_REG {
    struct {
        BCS_EIR_BITS error_bits;  ///< [15:0]  Hardware-detected error status (W1C)
        u16 mask;                 ///< [31:16] Mask field (WO)
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(EIR_REG) == 4);

#endif  // VESPERAOS_ERROR_REGS_H
