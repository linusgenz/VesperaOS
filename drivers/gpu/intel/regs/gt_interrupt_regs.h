// gt_interrupt_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.05.26.
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

#ifndef VESPERAOS_GT_INTERRUPT_REGS_H
#define VESPERAOS_GT_INTERRUPT_REGS_H

#include "interrupt_regs.h"

#define GT_REG(g, r) (&(g)->r)

// ============================================================================
// Gen8+ / KBL GT Interrupt Registers
// Reference: IHD-OS-KBL-Vol 2c — GT Interrupt 0 (0x44300)
//
// The GT0 interrupt register set manages hardware interrupt signals for both
// the Render Command Streamer (RCS) and Blitter Command Streamer (BCS) within
// a single 32-bit register word:
//   - Bits [15:0]  : RCS Interrupt Control/Status flags (RCS_ICR_BITS)
//   - Bits [31:16] : BCS Interrupt Control/Status flags (BCS_ICR_BITS)
//
// Register Layout per Group (16 bytes):
//   +0x0 : ISR (Interrupt Status Register)
//   +0x4 : IMR (Interrupt Mask Register)
//   +0x8 : IIR (Interrupt Identity Register)
//   +0xC : IER (Interrupt Enable Register)
//
// MMIO Base Addresses:
//   GT0 = 0x44300 | GT1 = 0x44310 | GT2 = 0x44320 | GT3 = 0x44330
// ============================================================================

/// RCS' MI_USER_INTERRUPT bit (RCS_ICR_BITS.user_irq, bit 0). NOT what this
/// driver uses for RCS completion — RCS signals via PIPE_CONTROL's Notify
/// Enable, which fires bit 4 instead (see GT0_RCS_PIPE_CONTROL_NOTIFY_BIT).
/// Only relevant if RCS ever emits an explicit MI_USER_INTERRUPT.
constexpr u32 GT0_RCS_USER_IRQ_BIT = 0;

/// RCS' PIPE_CONTROL Notify Interrupt bit (RCS_ICR_BITS.pipe_control_notify,
/// bit 4). This is what this driver actually unmasks/enables for RCS
/// completion — triggered by PIPE_CONTROL.notify_enable (PRM Vol 2a DWord 1
/// bit 8, a different bit-numbering space — same GT0 register, unrelated
/// bit index).
constexpr u32 GT0_RCS_PIPE_CONTROL_NOTIFY_BIT = 4;

/// BCS' MI_USER_INTERRUPT bit (BCS_ICR_BITS.user_irq, bit 16). This IS what
/// this driver uses for BCS completion — BCS emits MI_USER_INTERRUPT
/// directly (see IntelBcs::emit_mi_flush).
constexpr u32 GT0_BCS_USER_IRQ_BIT = 16;
/**
 * @brief Combined 32-bit layout for GT0 Interrupt Registers containing both RCS and BCS.
 */
union GT0_ICR_BITS {
    RCS_ICR_BITS rcs; ///< Bits [15:0]  : Render CS interrupts
    BCS_ICR_BITS bcs; ///< Bits [31:16] : Blitter CS interrupts
} __attribute__((packed));

static_assert(sizeof(GT0_ICR_BITS) == 4, "GT0_ICR_BITS must be exactly 32 bits");

union GT0_ISR_REG {
    GT0_ICR_BITS bits;
    u32 raw;
};

union GT0_IMR_REG {
    GT0_ICR_BITS bits;
    u32 raw;
};

union GT0_IIR_REG {
    GT0_ICR_BITS bits;
    u32 raw;
};

union GT0_IER_REG {
    GT0_ICR_BITS bits;
    u32 raw;
};

static_assert(sizeof(GT0_ISR_REG) == 4);
static_assert(sizeof(GT0_IMR_REG) == 4);
static_assert(sizeof(GT0_IIR_REG) == 4);
static_assert(sizeof(GT0_IER_REG) == 4);

/**
 * @brief One GT interrupt register group mapped directly onto MMIO.
 *
 * Each group occupies 16 bytes: ISR / IMR / IIR / IER at offsets +0/+4/+8/+C.
 * The register format is BCS_ICR_BITS: bits [15:0] are the unused RCS half;
 * bits [31:16] carry the BCS interrupt fields.
 *
 * @note IIR is W1C — never use read-modify-write on it.
 * @note IMR: 1 = masked.  Write the full value directly; do NOT use |= on MMIO.
 * @note IER: write the full desired value directly; do NOT use |= on MMIO.
 */
struct GT_INTR_REGS {
    GT0_ISR_REG isr;  ///< +0x0  GT Interrupt Status  (RO)
    GT0_IMR_REG imr;  ///< +0x4  GT Interrupt Mask    (R/W) (1 = masked)
    GT0_IIR_REG iir;  ///< +0x8  GT Interrupt Identity (R/WC)
    GT0_IER_REG ier;  ///< +0xC  GT Interrupt Enable   (R/W) (1 = enabled)
};

static_assert(sizeof(GT_INTR_REGS) == 16);

#endif  // VESPERAOS_GT_INTERRUPT_REGS_H
