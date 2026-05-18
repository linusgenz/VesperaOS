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

// ============================================================================
// Gen8+ / KBL GT Interrupt Registers
// IHD-OS-KBL-Vol 2c — GT Interrupt 0 Definition
// Base addresses: GT0=0x44300, GT1=0x44310, GT2=0x44320, GT3=0x44330
// Layout per group: +0=ISR, +4=IMR, +8=IIR, +C=IER
// ============================================================================

/**
 * @brief Bit layout for GT Interrupt 0 registers.
 *
 * Bits [15:0]  = RCS (Render Command Streamer).
 * Bits [31:16] = BCS (Blitter Command Streamer).
 * Layout mirrors GT Interrupt 1 (VCS1/VCS2) from Vol 2c §GT_1_INTERRUPT,
 * with RCS in the lower half and BCS in the upper half.
 */
union GT0_IRQ_BITS {
    struct {
        // ---- RCS [15:0] ----
        u32 rcs_user_irq : 1;         ///< [0]  MI User Interrupt
        u32 rcs_spare1 : 1;           ///< [1]
        u32 rcs_spare2 : 1;           ///< [2]
        u32 rcs_error : 1;            ///< [3]  Error Interrupt
        u32 rcs_flush_dw_notify : 1;  ///< [4]  MI Flush DW Notify
        u32 rcs_reserved5 : 1;        ///< [5]  Reserved
        u32 rcs_watchdog : 1;         ///< [6]  Watchdog Counter Expired
        u32 rcs_spare7 : 1;           ///< [7]
        u32 rcs_ctx_switch : 1;       ///< [8]  Context Switch Interrupt
        u32 rcs_reserved9 : 1;        ///< [9]  Reserved
        u32 rcs_spare10 : 1;          ///< [10]
        u32 rcs_wait_semaphore : 1;   ///< [11] Wait On Semaphore
        u32 rcs_spare12 : 4;          ///< [15:12]
        // ---- BCS [31:16] ----
        u32 bcs_user_irq : 1;         ///< [16] MI User Interrupt — fired by MI_USER_INTERRUPT cmd
        u32 bcs_spare17 : 1;          ///< [17]
        u32 bcs_spare18 : 1;          ///< [18]
        u32 bcs_error : 1;            ///< [19] Error Interrupt
        u32 bcs_flush_dw_notify : 1;  ///< [20] MI Flush DW Notify
        u32 bcs_reserved21 : 1;       ///< [21] Reserved
        u32 bcs_watchdog : 1;         ///< [22] Watchdog Counter Expired
        u32 bcs_spare23 : 1;          ///< [23]
        u32 bcs_ctx_switch : 1;       ///< [24] Context Switch Interrupt
        u32 bcs_reserved25 : 1;       ///< [25] Reserved
        u32 bcs_spare26 : 1;          ///< [26]
        u32 bcs_wait_semaphore : 1;   ///< [27] Wait On Semaphore
        u32 bcs_spare28 : 4;          ///< [31:28]
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(GT0_IRQ_BITS) == 4);

/**
 * @brief One GT interrupt register group mapped directly onto MMIO.
 *
 * Each GT group occupies 16 bytes: ISR/IMR/IIR/IER at offsets +0/+4/+8/+C.
 * Confirmed by Vol 2c GT_1_INTERRUPT definition (0x44310–0x4431C).
 *
 * @note IIR is write-1-to-clear — never use read-modify-write on it.
 * @note IMR: 1 = masked (interrupt suppressed).
 * @note IER: write the full desired value directly; do NOT use |= on MMIO.
 */
struct GT_INTR_REGS {
    volatile u32 isr;  ///< +0x0  Status        (read-only; clears when IIR is cleared).
    volatile u32 imr;  ///< +0x4  Mask           (1 = masked).
    volatile u32 iir;  ///< +0x8  Identity       (write 1 to clear pending bit).
    volatile u32 ier;  ///< +0xC  Enable         (1 = enabled; write full value, not |=).
} __attribute__((packed));

static_assert(sizeof(GT_INTR_REGS) == 16);

/**
 * @brief Master Interrupt Control register (MMIO 0x44200).
 *
 * IHD-OS-KBL-Vol 2c — MASTER_INT_CTL (0x44200)
 *
 * @note Bit 31 (master_enable) MUST be set before any
 *       interrupt reaches the PCI interrupt path.
 * @note Bits [30:0] are read-only pending flags – never write to them.
 *       Only exception: Bit 31 (R/W).
 * @note To rearm after the ISR: write raw = (1u<<31),
 *       no RMW — the pending bits reset themselves.
 */
union MASTER_INT_CTL {
    struct {
        u32 render_pending : 1;     ///< [0]  RCS Interrupts Pending         (RO)
        u32 blitter_pending : 1;    ///< [1]  BCS Interrupts Pending         (RO)
        u32 vcs1_pending : 1;       ///< [2]  VCS1 Interrupts Pending        (RO)
        u32 vcs2_pending : 1;       ///< [3]  VCS2 Interrupts Pending        (RO)
        u32 gtpm_pending : 1;       ///< [4]  GTPM Interrupts Pending        (RO)
        u32 reserved5 : 1;          ///< [5]  Reserved
        u32 vebox_pending : 1;      ///< [6]  VEBox Interrupts Pending       (RO)
        u32 reserved7_15 : 9;       ///< [15:7] Reserved
        u32 de_pipe_a_pending : 1;  ///< [16] DE Pipe A Interrupts Pending   (RO)
        u32 de_pipe_b_pending : 1;  ///< [17] DE Pipe B Interrupts Pending   (RO)
        u32 de_pipe_c_pending : 1;  ///< [18] DE Pipe C Interrupts Pending   (RO)
        u32 reserved19 : 1;         ///< [19] Reserved
        u32 de_port_pending : 1;    ///< [20] DE Port Interrupts Pending     (RO)
        u32 reserved21 : 1;         ///< [21] Reserved
        u32 de_misc_pending : 1;    ///< [22] DE Misc Interrupts Pending     (RO)
        u32 de_pch_pending : 1;     ///< [23] DE PCH Interrupts Pending      (RO)
        u32 audio_pending : 1;      ///< [24] Audio Codec Interrupts Pending (RO)
        u32 reserved25_29 : 5;      ///< [29:25] Reserved
        u32 pcu_pending : 1;        ///< [30] PCU Interrupts Pending         (RO)
        u32 master_enable : 1;      ///< [31] Master Interrupt Enable        (R/W)
    } __attribute__((packed));
    u32 raw;
};

static_assert(sizeof(MASTER_INT_CTL) == 4);

/// MMIO offset of the Master Interrupt Control register.
constexpr u32 GEN8_MASTER_INT_CTL_OFFSET = 0x44200;

/// MMIO offset of the GT0 interrupt register group (RCS + BCS).
constexpr u32 GEN8_GT0_INTR_BASE = 0x44300;

#endif  // VESPERAOS_GT_INTERRUPT_REGS_H
