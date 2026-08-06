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

#include <vespera/types.h>

struct RCS_ICR_BITS {
    u32 user_irq : 1;        ///< [0]  MI_USER_INTERRUPT
    u32 reserved1 : 1;       ///< [1]  MBZ
    u32 reserved2 : 1;       ///< [2]  MBZ
    u32 master_error : 1;    ///< [3]  Render Master Error
    u32 pipe_control_notify : 1; ///< [4] PIPE_CONTROL Notify
    u32 reserved5 : 1;       ///< [5]  MBZ
    u32 timeout : 1;         ///< [6]  Timeout Counter Expired
    u32 page_fault : 1;      ///< [7]  Page Fault
    u32 ctx_switch : 1;      ///< [8]  Context Switch Interrupt
    u32 invalid_tile : 1;    ///< [9]  TR Invalid Tile Detection
    u32 l3_counter : 1;      ///< [10] L3 Counter Save Interrupt
    u32 wait_sem : 1;        ///< [11] Wait on Semaphore
    u32 reserved12_15 : 4;   ///< [15:12] MBZ
    u32 reserved16_31 : 16;  ///< [31:16] MBZ (Must Be Zero)
} __attribute__((packed));
static_assert(sizeof(RCS_ICR_BITS) == 4);


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
struct BCS_ICR_BITS {
    u32 reserved0_15 : 16;  ///< [15:0]  MBZ
    u32 user_irq : 1;       ///< [16]    Blitter Command Parser User Interrupt — fired by MI_USER_INTERRUPT
    u32 reserved17_18 : 2;  ///< [18:17] MBZ

    /**
     * @brief Blitter Command Parser Master Error [19]
     *
     * Set when hardware detects an error condition (Page Table Error,
     * Instruction Parser Error).  Further detail via ESR / EIR.
     *
     * Clear sequence: write 1 to the relevant EIR bit, then write 1 here.
     */
    u32 master_error : 1;  ///< [19]
    u32 mi_flush_dw : 1;   ///< [20]    MI_FLUSH_DW Notify — fires after the associated store QW completes
    u32 reserved21 : 1;    ///< [21]    MBZ

    /**
     * @brief Timeout Counter Expired [22]
     *
     * Set when the BCS timeout counter reaches its threshold.
     *
     * @warning NOT POR.  MUST NOT be unmasked in IMR / HWSTAM.
     */
    u32 timeout : 1;        ///< [22]
    u32 reserved23 : 1;     ///< [23]    MBZ
    u32 ctx_switch : 1;     ///< [24]    Context Switch complete — requires Exec-List Enable
    u32 reserved25_26 : 2;  ///< [26:25] MBZ

    /**
     * @brief Wait on Semaphore [27]
     *
     * Exec-List mode: set when MI_SEMAPHORE_WAIT fails and
     * "Inhibit Synchronous Context Switch" is active.
     * Ring-Buffer mode: set when MI_SEMAPHORE_WAIT fails.
     */
    u32 wait_sem : 1;       ///< [27]
    u32 reserved28_31 : 4;  ///< [31:28] MBZ
} __attribute__((packed));
static_assert(sizeof(BCS_ICR_BITS) == 4);


union HWSTAM_REG {
    u32 raw;
    RCS_ICR_BITS rcs_bits;
    BCS_ICR_BITS bcs_bits;
};

/**
 * @brief BCS Interrupt Mask Register (IMR).
 *
 * Controls which interrupt sources are forwarded to the GT interrupt path.
 *
 * @note 1 = masked (suppressed); 0 = unmasked (forwarded).
 * @note Default reset value = 0xFFFFFFFF (all masked).
 * @note Write the full desired value directly; do NOT use |= on MMIO.
 *
 * MMIO: 0x220A8 (BCS_IMR)
 */
union BCS_IMR_REG {
    BCS_ICR_BITS bits;
    u32 raw;
};


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

union DE_PIPE_INTERRUPT {
    struct {
        u32 vblank : 1;            ///< [0]     Vertical Blank — active high for duration of vblank interval
        u32 vsync : 1;             ///< [1]     Vertical Sync  — active high for duration of vsync interval
        u32 scanline : 1;          ///< [2]     Scan Line Event — active high pulse on scanline event
        u32 plane1_flip_done : 1;  ///< [3]     Plane 1 Flip Done
        u32 plane2_flip_done : 1;  ///< [4]     Plane 2 Flip Done
        u32 plane3_flip_done : 1;  ///< [5]     Plane 3 Flip Done — not present on all pipes
        u32 plane4_flip_done : 1;  ///< [6]     Plane 4 Flip Done — not present on all pipes
        u32 plane1_gtt_fault : 1;  ///< [7]     Plane 1 GTT Fault
        u32 plane2_gtt_fault : 1;  ///< [8]     Plane 2 GTT Fault
        u32 plane3_gtt_fault : 1;  ///< [9]     Plane 3 GTT Fault — not present on all pipes
        u32 plane4_gtt_fault : 1;  ///< [10]    Plane 4 GTT Fault — not present on all pipes
        u32 cursor_gtt_fault : 1;  ///< [11]    Cursor GTT Fault
        u32 dpst_histogram : 1;    ///< [12]    DPST Histogram Event
        u32 unused13_15 : 3;       ///< [15:13] Currently unused interrupt sources
        u32 reserved16_19 : 4;     ///< [19:16] MBZ
        u32 unused20_27 : 8;       ///< [27:20] Currently unused interrupt sources
        u32 reserved28_29 : 2;     ///< [29:28] MBZ
        u32 unused30 : 1;          ///< [30]    Currently unused interrupt source

        /**
         * @brief Underrun [31]
         *
         * Active high pulse when an underrun occurs on the transcoder attached
         * to this pipe.
         */
        u32 underrun : 1;  ///< [31] Underrun
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(DE_PIPE_INTERRUPT) == 4);

using DE_PIPE_ISR = DE_PIPE_INTERRUPT;  ///< RO — raw interrupt line states; do not write
using DE_PIPE_IMR = DE_PIPE_INTERRUPT;  ///< 1 = masked; 0 = forwarded.  Write full value, no |= on MMIO
using DE_PIPE_IIR = DE_PIPE_INTERRUPT;  ///< Sticky; write 1 to clear
using DE_PIPE_IER = DE_PIPE_INTERRUPT;  ///< 1 = enabled; 0 = disabled

// ============================================================================
// DE Pipe Interrupt register MMIO offsets — Pipe A
// ============================================================================

constexpr u32 DE_PIPE_A_ISR = 0x44400;
constexpr u32 DE_PIPE_A_IMR = 0x44404;
constexpr u32 DE_PIPE_A_IIR = 0x44408;
constexpr u32 DE_PIPE_A_IER = 0x4440C;

// ============================================================================
// DE Pipe Interrupt register MMIO offsets — Pipe B
// ============================================================================

constexpr u32 DE_PIPE_B_ISR = 0x44410;
constexpr u32 DE_PIPE_B_IMR = 0x44414;
constexpr u32 DE_PIPE_B_IIR = 0x44418;
constexpr u32 DE_PIPE_B_IER = 0x4441C;

// ============================================================================
// DE Pipe Interrupt register MMIO offsets — Pipe C
// ============================================================================

constexpr u32 DE_PIPE_C_ISR = 0x44420;
constexpr u32 DE_PIPE_C_IMR = 0x44424;
constexpr u32 DE_PIPE_C_IIR = 0x44428;
constexpr u32 DE_PIPE_C_IER = 0x4442C;

#endif  // VESPERAOS_INTERRUPT_REGS_H