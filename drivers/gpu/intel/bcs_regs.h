// bcs_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.05.26.
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
#ifndef VESPERAOS_RING_REGS_H
#define VESPERAOS_RING_REGS_H
#include "error_regs.h"
#include "interrupt_regs.h"

constexpr u32 BCS_RING_BASE = 0x22000;

union RING_BUFFER_CTL {
    struct {
        u32 ring_enable : 1;       ///< [0]   Ring Buffer Enable
        u32 auto_report_head : 2;  ///< [2:1] Automatic Report Head Pointer
        u32 reserved3_7 : 5;       ///< [7:3] MBZ
        u32 reserved8 : 1;         ///< [8]   MBZ
        u32 reserved9 : 1;         ///< [9]   MBZ
        u32 semaphore_wait : 1;    ///< [10]  Semaphore Wait Status (W1C)
        u32 rb_wait : 1;           ///< [11]  WAIT_FOR_EVENT Status (W1C)
        u32 buffer_length : 9;     ///< [20:12] Ring Length in 4KB pages - 1
        u32 reserved21_31 : 11;    ///< [31:21] MBZ
    } __attribute__((packed));

    u32 raw;

    enum AutoReport : u32 {
        MI_AUTOREPORT_OFF = 0,
        MI_AUTOREPORT_64KB = 1,
        MI_AUTOREPORT_4KB = 2,
        MI_AUTOREPORT_128KB = 3,
    };

    [[nodiscard]] constexpr u32 ring_size_bytes() const {
        return (buffer_length + 1) * 4096;
    }

    constexpr void set_ring_size_bytes(u32 bytes) {
        buffer_length = (bytes / 4096) - 1;
    }
};

union RING_BUFFER_TAIL {
    struct {
        u32 reserved0_2 : 3;     ///< [2:0]   MBZ
        u32 tail_offset : 18;    ///< [20:3]  Tail Offset (QWord aligned)
        u32 reserved21_30 : 10;  ///< [30:21] MBZ
        u32 reserved31 : 1;      ///< [31]    Reserved
    } __attribute__((packed));

    u32 raw;

    [[nodiscard]] constexpr u32 tail_offset_bytes() const volatile {
        return tail_offset << 3;
    }

    constexpr void set_tail_offset_bytes(u32 bytes) volatile {
        tail_offset = bytes >> 3;
    }
};

union RING_BUFFER_HEAD {
    struct {
        u32 reserved0_1 : 2;   ///< [1:0]   MBZ
        u32 head_offset : 19;  ///< [20:2]  Head Offset (DWord aligned)
        u32 wrap_count : 11;   ///< [31:21] Ring Buffer Wrap Count
    } __attribute__((packed));

    u32 raw;

    [[nodiscard]] constexpr u32 head_offset_bytes() volatile const {
        return head_offset << 2;
    }

    constexpr void set_head_offset_bytes(u32 bytes) {
        head_offset = bytes >> 2;
    }
};

union RING_BUFFER_START {
    struct {
        u32 reserved0_11 : 12;  ///< [11:0]  MBZ
        u32 start_addr : 20;    ///< [31:12] Ring Buffer Start Address (4KB aligned)
    } __attribute__((packed));

    u32 raw;

    [[nodiscard]] constexpr u32 start_addr_bytes() const {
        return start_addr << 12;
    }

    constexpr void set_start_addr_bytes(u32 bytes) {
        start_addr = bytes >> 12;
    }
};

union BCS_SWCTRL {
    struct {
        u32 mask : 16;           ///< [31:16] Mask (WO)
        u32 mbz0 : 12;           ///< [15:4]  Must be zero
        u32 shrink_cache : 1;    ///< [3]     Shrink Blitter Cache
        u32 not_invalidate : 1;  ///< [2]     Do not invalidate Blitter cache on flush
        u32 tile_y_dst : 1;      ///< [1]     Force Tile Y Destination
        u32 tile_y_src : 1;      ///< [0]     Force Tile Y Source
    } __attribute__((packed));

    u32 raw;
};

union HWS_PGA {
    struct {
        u32 reserved0_11 : 12;  ///< [11:0]  MBZ
        u32 address : 20;       ///< [31:12] Hardware Status Page Address (4KB aligned)
    } __attribute__((packed));

    u32 raw;

    [[nodiscard]] constexpr u32 address_bytes() const {
        return address << 12;
    }

    constexpr void set_address_bytes(u32 addr) {
        address = addr >> 12;
    }
};
static_assert(sizeof(HWS_PGA) == 4);

union HWSTAM {
    BCS_ICR_BITS bits;
    u32 raw;
};
static_assert(sizeof(HWSTAM) == 4);

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
    HWSTAM hwstam;

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
#endif  // VESPERAOS_RING_REGS_H
