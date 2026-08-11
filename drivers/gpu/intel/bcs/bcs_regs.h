// bcs_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.08.26.
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

#ifndef VESPERAOS_BCS_REGS_H
#define VESPERAOS_BCS_REGS_H

#include "../regs/ring_regs.h"

constexpr u32 BCS_RING_BASE = 0x22000;

/**
 * @brief BCS MMIO register window - base = MMIO_BAR + 0x22000.
 *
 * The layout covers all registers listed in the BCS Power Context Image table.
 * Offset sanity is enforced by the `static_assert` block below the struct.
 *
 * @warning The struct is **not** `__attribute__((packed))` globally; the
 *          static_asserts below are the authoritative layout check.  If any
 *          assert fires, a padding field has the wrong size.
 */
struct BCS_REGS {
    /* 0x000 */
    u8 _pad000[0x30];

    /* 0x030 */
    RING_BUFFER_TAIL ring_tail;
    RING_BUFFER_HEAD ring_head;
    RING_BUFFER_START ring_start;
    RING_BUFFER_CTL ring_ctl;

    /* 0x040 */
    u32 sync0;
    u32 sync1;
    u32 sync2;

    /* 0x04C */
    u8 _pad04C[0x04];

    u32 psmi_ctl; /* 0x050 */
    u32 max_idle; /* 0x054 */

    /* 0x058 -> 0x080 */
    u8 _pad058[0x28];

    /* 0x080 */
    HWS_PGA hwsp;

    /* 0x084 -> 0x098 */
    u8 _pad084[0x14];

    /* 0x098 */
    HWSTAM_REG hwstam;

    /* 0x09C -> 0x0A8 */
    u8 _pad09C[0x0C];

    /* 0x0A8 */
    BCS_IMR_REG imr;

    /* 0x0AC -> 0x0B0 */
    u8 _pad0AC[0x04];

    /* 0x0B0 */
    EIR_REG eir;

    /* 0x0B4 */
    EMR_REG emr;

    u8 _pad0B8[0x148];

    /* 0x200 (0x22200 MMIO) */
    BCS_SWCTRL swctrl;

    u8 _pad204[0x0C]; /* 0x204 - 0x20F */
};

static_assert(offsetof(BCS_REGS, ring_tail) == 0x030);
static_assert(offsetof(BCS_REGS, ring_head) == 0x034);
static_assert(offsetof(BCS_REGS, ring_start) == 0x038);
static_assert(offsetof(BCS_REGS, ring_ctl) == 0x03C);
static_assert(offsetof(BCS_REGS, hwsp) == 0x080);
static_assert(offsetof(BCS_REGS, hwstam) == 0x098);
static_assert(offsetof(BCS_REGS, imr) == 0x0A8);
static_assert(offsetof(BCS_REGS, eir) == 0x0B0);
static_assert(offsetof(BCS_REGS, emr) == 0x0B4);
static_assert(offsetof(BCS_REGS, swctrl) == 0x200);

#endif //VESPERAOS_BCS_REGS_H
