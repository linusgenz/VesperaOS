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
// IHD-OS-KBL-Vol 2c — GT Interrupt 0 (0x44300)
//
// The GT0 register group covers RCS (bits [15:0]) and BCS (bits [31:16]).
// Bit positions [31:16] map 1:1 to BCS_ICR_BITS — the lower 16 bits carry
// the RCS half which is unused by this driver (MBZ / ignored on reads).
//
// Layout per group:  +0 = ISR,  +4 = IMR,  +8 = IIR,  +C = IER
// Base addresses:    GT0 = 0x44300,  GT1 = 0x44310,  GT2 = 0x44320,  GT3 = 0x44330
// ============================================================================

union GT0_ISR_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

union GT0_IMR_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

union GT0_IIR_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

union GT0_IER_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

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
} __attribute__((packed));

static_assert(sizeof(GT_INTR_REGS) == 16);

// ============================================================================
// Master Interrupt Control
// IHD-OS-KBL-Vol 2c — MASTER_INT_CTL (0x44200)
// ============================================================================

/**
 * @brief Master Interrupt Control register (MASTER_INT_CTL).
 *
 * @note Bit 31 (master_enable) MUST be set before any interrupt reaches
 *       the PCI interrupt path.
 * @note Bits [30:0] are RO pending flags — never write to them.
 * @note To re-arm after ISR handling: write raw = (1u << 31).
 *       No RMW — the pending bits self-clear.
 */
union MASTER_INT_CTL {
    struct {
        u32 render_pending : 1;     ///< [0]     RCS Interrupts Pending         (RO)
        u32 blitter_pending : 1;    ///< [1]     BCS Interrupts Pending         (RO)
        u32 vcs1_pending : 1;       ///< [2]     VCS1 Interrupts Pending        (RO)
        u32 vcs2_pending : 1;       ///< [3]     VCS2 Interrupts Pending        (RO)
        u32 gtpm_pending : 1;       ///< [4]     GTPM Interrupts Pending        (RO)
        u32 reserved5 : 1;          ///< [5]     MBZ
        u32 vebox_pending : 1;      ///< [6]     VEBox Interrupts Pending       (RO)
        u32 reserved7_15 : 9;       ///< [15:7]  MBZ
        u32 de_pipe_a_pending : 1;  ///< [16]    DE Pipe A Interrupts Pending   (RO)
        u32 de_pipe_b_pending : 1;  ///< [17]    DE Pipe B Interrupts Pending   (RO)
        u32 de_pipe_c_pending : 1;  ///< [18]    DE Pipe C Interrupts Pending   (RO)
        u32 reserved19 : 1;         ///< [19]    MBZ
        u32 de_port_pending : 1;    ///< [20]    DE Port Interrupts Pending     (RO)
        u32 reserved21 : 1;         ///< [21]    MBZ
        u32 de_misc_pending : 1;    ///< [22]    DE Misc Interrupts Pending     (RO)
        u32 de_pch_pending : 1;     ///< [23]    DE PCH Interrupts Pending      (RO)
        u32 audio_pending : 1;      ///< [24]    Audio Codec Interrupts Pending (RO)
        u32 reserved25_29 : 5;      ///< [29:25] MBZ
        u32 pcu_pending : 1;        ///< [30]    PCU Interrupts Pending         (RO)
        u32 master_enable : 1;      ///< [31]    Master Interrupt Enable        (R/W)
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(MASTER_INT_CTL) == 4);

/// MMIO offset of the Master Interrupt Control register.
constexpr u32 GEN8_MASTER_INT_CTL_OFFSET = 0x44200;

/// MMIO offset of the GT0 interrupt register group (RCS + BCS).
constexpr u32 GEN8_GT0_INTR_BASE = 0x44300;

#endif  // VESPERAOS_GT_INTERRUPT_REGS_H
