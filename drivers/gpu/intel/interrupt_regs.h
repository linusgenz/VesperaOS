// interrupt_regs.h
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

#ifndef VESPERAOS_INTERRUPT_REGS_H
#define VESPERAOS_INTERRUPT_REGS_H

// IHD-OS-KBL-Vol 2c p.1080  — Interrupt Control Registers (BCS)
// IHD-OS-KBL-Vol 2d p.38    — Bit Definition for Interrupt Control Registers - Blitter

// ============================================================================
// Canonical bit layout — BCS Interrupt Control Registers
// ============================================================================

/**
 * @brief BCS Interrupt Control Register bit layout.
 *
 * Shared format for ISR, IMR, IIR, IER, and HWSTAM of the Blitter Command
 * Streamer.  All five registers map the same 32-bit field; the semantics
 * of each bit differ by register (status / mask / identity / enable / HW-status-mask).
 *
 * IHD-OS-KBL-Vol 2d — "Bit Definition for Interrupt Control Registers - Blitter"
 *
 * @note Bits [15:0] and all reserved fields are MBZ on writes; RO in hardware.
 */
struct  BCS_ICR_BITS {
    u32 reserved0_15 : 16;  ///< [15:0]  MBZ

    /**
     * @brief Blitter Command Parser User Interrupt [16]
     *
     * Set when MI_USER_INTERRUPT is executed on the BCS.
     * Instruction execution is not halted; use MI_STORE_DATA to associate
     * a specific meaning with the interrupt.
     */
    u32 user_irq : 1;  ///< [16]

    u32 reserved17_18 : 2;  ///< [18:17] MBZ

    /**
     * @brief Blitter Command Parser Master Error [19]
     *
     * Set when hardware detects an error condition.
     * Further detail is available via ESR / EIR.
     * Clear sequence: write 1 to the relevant EIR bit, then write 1 here.
     *
     * Sources:
     *   - Page Table Error
     *   - Instruction Parser Error
     */
    u32 master_error : 1;  ///< [19]

    /**
     * @brief MI_FLUSH_DW Notify Interrupt [20]
     *
     * Optionally fired by MI_FLUSH_DW.  The associated store QW completes
     * before the interrupt fires.
     */
    u32 mi_flush_dw : 1;  ///< [20]

    u32 reserved21 : 1;  ///< [21]    MBZ

    /**
     * @brief Timeout Counter Expired [22]
     *
     * Set when the BCS timeout counter reaches the threshold.
     *
     * @warning NOT POR.  MUST NOT be unmasked (enabled) in IMR / HWSTAM.
     */
    u32 timeout : 1;  ///< [22]

    u32 reserved23 : 1;  ///< [23]    MBZ

    /**
     * @brief Context Switch Interrupt [24]
     *
     * Set after a context switch completes.
     * Requires Exec-List Enable to be set.
     */
    u32 ctx_switch : 1;  ///< [24]

    u32 reserved25_26 : 2;  ///< [26:25] MBZ

    /**
     * @brief Wait on Semaphore [27]
     *
     * Exec-List mode: set when MI_SEMAPHORE_WAIT fails and
     * "Inhibit Synchronous Context Switch" is active.
     * Ring-Buffer mode: set when MI_SEMAPHORE_WAIT fails.
     */
    u32 wait_sem : 1;  ///< [27]

    u32 reserved28_31 : 4;  ///< [31:28] MBZ
} __attribute__((packed));

static_assert(sizeof(BCS_ICR_BITS) == 4);

/**
 * @brief BCS Interrupt Mask Register (IMR).
 *
 * Controls which interrupt sources are forwarded to the GT interrupt path.
 *
 * @note 1 = masked (suppressed); 0 = unmasked (forwarded).
 * @note Default reset value = 0xFFFFFFFF (all masked).
 * @note Write the full desired value directly; do NOT use |= on MMIO.
 *
 * MMIO: 0x22304 (BCS_IMR)
 */
union BCS_IMR_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

// ============================================================================
// HWSTAM — Hardware Status Mask Register
// IHD-OS-KBL-Vol 2c — HWSTAM (0x22098)
// ============================================================================

/**
 * @brief Hardware Status Mask Register (HWSTAM).
 *
 * Controls which ISR bits trigger a Hardware Status Write (PCI write cycle
 * into the Hardware Status Page).  Shares the same bit layout as the other
 * BCS Interrupt Control Registers (BCS_ICR_BITS).
 *
 * @note 0 = unmasked — ISR bit change writes to HWSP.
 * @note 1 = masked   — ISR bit change suppressed (default).
 * @note To write an interrupt to HWSP the corresponding IMR bit must also
 *       be clear (unmasked).
 * @note At most 1 bit should be unmasked at any given time.
 * @note Default reset value = 0xFFFFFFFF.
 * @note Reserved bits are RO; must be preserved (written as 1).
 *
 */
union HWSTAM_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};

static_assert(sizeof(HWSTAM_REG) == 4);

/// MMIO offset of the BCS Hardware Status Mask Register.
constexpr u32 BCS_HWSTAM_OFFSET = 0x22098;

/// MMIO offset of the BCS Interrupt Mask Register.
constexpr u32 BCS_IMR_OFFSET = 0x220A8;


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


#endif  // VESPERAOS_INTERRUPT_REGS_H