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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_ERROR_REGS_H
#define VESPERAOS_ERROR_REGS_H

/**
 * @brief Error Mask Register (EMR).
 *
 * Controls which Engine Status Register (ESR) error bits are propagated
 * into the Engine Interrupt Register (EIR).
 *
 * IHD-OS-KBL-Vol 2c — EMR (Error Mask Register)
 *
 * @note Bit value semantics:
 *       0 = unmasked  -> error propagates into EIR
 *       1 = masked    -> error suppressed
 *
 * @note Bits [31:8] are reserved, read-only in HW, and MUST remain '1'.
 *       Software must preserve/set them to 1 on writes.
 *
 * @note Default reset value = 0xFFFFFFFF
 *
 * @note EMR only controls propagation into EIR / master interrupt logic.
 *       It does NOT prevent the underlying ESR status bit from being set.
 */
union EMR_REG {
    struct {
        /**
         * @brief Error mask bits.
         *
         * One bit per ESR error condition.
         *
         * 0 = error is reported into EIR
         * 1 = error is masked
         *
         * Exact bit meanings depend on the engine-specific
         * Hardware-Detected Error Bits table.
         */
        u32 error_mask_bits : 8;

        /**
         * @brief Reserved.
         *
         * Not implemented in hardware.
         * Must always be written as 1.
         */
        u32 reserved : 24;
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(EMR_REG) == 4);

struct BCS_EIR_ERROR_BITS {
    /**
     * @brief Instruction Error [0]
     *
     * Fatal error:
     * - unsupported client ID
     * - invalid instruction decode
     * - deprecated MI opcodes
     *
     * NOTE: cannot be cleared except reset
     */
    u16 instruction_error : 1;

    u16 reserved1 : 1;

    /**
     * @brief Command Privilege Violation [2]
     *
     * Privileged command in non-privileged batch buffer.
     * Hardware converts command to NOOP and continues parsing.
     */
    u16 privilege_violation : 1;

    u16 reserved3_15 : 13;  ///< bits 15:3 MBZ
} __attribute__((packed));

static_assert(sizeof(BCS_EIR_ERROR_BITS) == 2);

/**
 * @brief Error Identity Register (EIR).
 *
 * Holds persistent error status bits that were unmasked via EMR.
 * A set bit indicates a hardware-detected error condition.
 *
 * Any set bit contributes to the Master Error condition in ISR.
 *
 * @note Writing 1 clears the corresponding error bit (W1C behavior).
 * @note Bit 0 (Instruction Error) and Bit 4 (Page Table Error)
 *       are fatal and cannot be cleared by software.
 */
union EIR_REG {
    struct {
        /**
         * @brief Error identity bits [15:0]
         *
         * Each bit represents a hardware-detected error condition.
         * 1 = error occurred (persistent until cleared)
         */
        BCS_EIR_ERROR_BITS bcs_errors;

        /**
         * @brief Mask / reserved write-only field [31:16]
         *
         * Used for clearing via write-1-to-clear semantics.
         * Must be written as 0 when not explicitly clearing bits.
         */
        u32 mask : 16;
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(EIR_REG) == 4);

#endif  // VESPERAOS_ERROR_REGS_H
