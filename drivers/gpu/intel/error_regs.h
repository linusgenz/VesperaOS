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

/**
 * @brief BCS Hardware-Detected Error Bit Definitions [15:0].
 */
struct BCS_ERROR_BITS {
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
    u16 instruction_error : 1; ///< [0]

    u16 reserved1 : 1; ///< [1]  MBZ

    /**
     * @brief Command Privilege Violation [2].
     *
     * A privileged command was issued from a non-privileged batch buffer.
     * Hardware converts the command to a NOOP and continues parsing.
     */
    u16 privilege_violation : 1; ///< [2]

    u16 reserved3_15 : 13; ///< [15:3] MBZ
} __attribute__((packed));

static_assert(sizeof(BCS_ERROR_BITS) == 2);

/**
 * @brief RCS Hardware-Detected Error Bit Definitions [15:0].
 *
 * Default Value: 0x00000000
 */
struct RCS_ERROR_BITS {
    /**
     * @brief Instruction Error [0].
     *
     * Fatal; set when the Renderer Instruction Parser detects an error
     * while parsing an instruction.
     *
     * @note Cannot be cleared except by reset.
     */
    u16 instruction_error : 1;  ///< [0] 1 = Instruction Error detected

    u16 reserved1 : 1;          ///< [1] MBZ

    /**
     * @brief Command Privilege Violation Error [2].
     *
     * This bit is set if a command classified as privileged is parsed
     * in a non-privileged batch buffer. The command will be converted
     * to a NOOP and parsing will continue.
     */
    u16 privilege_violation : 1;  ///< [2]

    u16 reserved3_15 : 13;        ///< [15:3] MBZ (Covers reserved bits 6:3, 7 and 15:8)
} __attribute__((packed));

static_assert(sizeof(RCS_ERROR_BITS) == 2);

// ============================================================================
// EIR — Error Identity Register
// ============================================================================

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
 * MMIO Addresses:
 *   - 0x020B0-0x020B3 (EIR_RCSUNIT)
 *   - 0x120B0-0x120B3 (EIR_VCSUNIT0)
 *   - 0x1A0B0-0x1A0B3 (EIR_VECSUNIT)
 *   - 0x1C0B0-0x1C0B3 (EIR_VCSUNIT1)
 *   - 0x220B0-0x220B3 (EIR_BCSUNIT)
 */
union EIR_REG {
    struct {
        union {
            u16 error_bits; ///< [15:0]  Hardware-detected error status (W1C)
            BCS_ERROR_BITS bcs_error_bits;
            RCS_ERROR_BITS rcs_error_bits;
        };
        u16 mask;                  ///< [31:16] Mask field (WO)
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(EIR_REG) == 4);

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
 *
 * MMIO Addresses:
 * - 0x020B4-0x020B7 (EMR_RCSUNIT)
 * - 0x120B4-0x120B7 (EMR_VCSUNIT0)
 * - 0x1A0B4-0x1A0B7 (EMR_VECSUNIT)
 * - 0x1C0B4-0x1C0B7 (EMR_VCSUNIT1)
 * - 0x220B4-0x220B7 (EMR_BCSUNIT)
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
        u32 error_mask : 8; ///< [7:0]

        /**
         * @brief Reserved [31:8].
         *
         * Not implemented in hardware.  MUST be written as 1.
         */
        u32 reserved : 24; ///< [31:8]  write-as-1
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(EMR_REG) == 4);

// ============================================================================
// ERR — Error Reporting Register
// ============================================================================

/**
 * @brief Error Reporting Register (ERR).
 *
 * @note Default Value: 0x00000000
 * @note Address: 0x0B42C
 */
union ERR_REG {
    struct {
        u32 reserved_0 : 1; ///< [0] Reserved (RO)

        /**
         * @brief Buffer full Error Slice 0 (BFFLERR0).
         * Set when all buffers are full, or if only 1 buffer is enabled then
         * when the buffer is full.
         */
        u32 buffer_full_error_slice_0 : 1; ///< [1] Access: R/W

        /**
         * @brief Write Expired Error slice 0 (WEERR0).
         * Set if DMA controller could not get a chance to push the write of
         * 64Bytes to LTISEQ and data gets clobbered.
         */
        u32 write_expire_error_slice_0 : 1; ///< [2] Access: R/W

        /**
         * @brief Second Content Buffer Ready slice 0 (SCNBFR0).
         * Set by HW when the buffer is completely filled up and cleared by the
         * driver when the contents are copied out of memory.
         */
        u32 second_buffer_ready_slice_0 : 1; ///< [3] Access: R/W

        /**
         * @brief First Content Buffer Ready 0 (FRSNTBFR0).
         * Set by HW when the buffer is completely filled up and cleared by the
         * driver when the contents are copied out of memory.
         */
        u32 first_content_buffer_ready_0 : 1; ///< [4] Access: R/W

        u32 reserved_31_5 : 27; ///< [31:5] Reserved (RO)
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(ERR_REG) == 4);

// ============================================================================
// ESR — Error Status Register
// ============================================================================

/**
 * @brief Error Status Register (ESR).
 *
 * The ESR register contains the current values of all Hardware-Detected Error
 * condition bits (these are all by definition persistent). The EMR register
 * selects which of these error conditions are reported in the persistent EIR
 * (i.e., set bits must be cleared by software) and thereby causing a Master
 * Error interrupt condition to be reported in the ISR.
 *
 * @note Access: RO
 * @note Default Value: 0x00000000
 *
 * MMIO Addresses:
 *   - 0x020B8-0x020BB (ESR_RCSUNIT)
 *   - 0x120B8-0x120BB (ESR_VCSUNIT0)
 *   - 0x1A0B8-0x1A0BB (ESR_VECSUNIT)
 *   - 0x1C0B8-0x1C0BB (ESR_VCSUNIT1)
 *   - 0x220B8-0x220BB (ESR_BCSUNIT)
 */
union ESR_REG {
    struct {
        /**
         * @brief Error Status Bits [15:0].
         *
         * Array of error condition bits. This register contains the
         * non-persistent values of all hardware-detected error condition bits.
         *
         * 1 = Error Condition Detected
         */
        union { ///< [15:0] RO
            u16 error_bits;
            BCS_ERROR_BITS bcs_error_bits;
            RCS_ERROR_BITS rcs_error_bits;
        };

        /**
         * @brief Reserved [31:16].
         */
        u16 reserved_16; ///< [31:16] MBZ
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(ESR_REG) == 4);

#endif  // VESPERAOS_ERROR_REGS_H
