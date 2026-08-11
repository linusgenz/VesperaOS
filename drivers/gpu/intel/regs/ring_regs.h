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


#endif  // VESPERAOS_RING_REGS_H
