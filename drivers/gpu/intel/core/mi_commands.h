// mi_commands.h
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

#ifndef VESPERAOS_MI_COMMANDS_H
#define VESPERAOS_MI_COMMANDS_H

#include <vespera/types.h>

namespace gpu::intel::core {

    /// Command client IDs für Command-Header.
    enum class CommandClient : u32 {
        Mi = 0x0,            // Memory Interface (MI_* commands)
        Processor2D = 0x2,   // 2D Blitter Engine (XY_* commands)
        Render3D = 0x3,      // 3D/Render Engine (3DSTATE_* commands)
    };

    /// MI Command Opcodes (Bits [28:23])
    enum MiOpcode : u32 {
        OPCODE_MI_NOOP = 0x00,
        OPCODE_MI_USER_INTERRUPT = 0x02,
        OPCODE_MI_WAIT_FOR_EVENT = 0x03,
    };

    constexpr u32 MI_NOOP = 0x00000000;

    // --- MI_USER_INTERRUPT ---
    union MI_USER_INTERRUPT_CMD {
        struct {
            u32 reserved : 23;  // [22:0] MBZ
            u32 opcode : 6;     // [28:23] MI Command Opcode = 0x02
            u32 client : 3;     // [31:29] Command Type = MI_COMMAND (0)
        } __attribute__((packed));
        u32 raw;
    };

    // --- MI_WAIT_FOR_EVENT ---
    union MI_WAIT_FOR_EVENT_CMD {
        struct {
            u32 display_plane_1_a_scan_line_wait : 1;       // [0]
            u32 display_plane_1_a_flip_pending_wait : 1;    // [1]
            u32 display_plane_2_a_flip_pending_wait : 1;    // [2]
            u32 display_plane_1_a_vertical_blank_wait : 1;  // [3]
            u32 reserved_5_4 : 2;                           // [5:4]   MBZ
            u32 display_plane_3_a_flip_pending_wait : 1;    // [6]
            u32 display_plane_3_b_flip_pending_wait : 1;    // [7]
            u32 display_plane_1_b_scan_line_wait : 1;       // [8]
            u32 display_plane_1_b_flip_pending_wait : 1;    // [9]
            u32 display_plane_2_b_flip_pending_wait : 1;    // [10]
            u32 display_plane_1_b_vertical_blank_wait : 1;  // [11]
            u32 reserved_13_12 : 2;                         // [13:12] MBZ
            u32 display_plane_1_c_scan_line_wait : 1;       // [14]
            u32 display_plane_1_c_flip_pending_wait : 1;    // [15]
            u32 display_plane_3_c_flip_pending_wait : 1;    // [16]
            u32 display_plane_4_a_flip_pending_wait : 1;    // [17]
            u32 display_plane_4_b_flip_pending_wait : 1;    // [18]
            u32 display_plane_4_c_flip_pending_wait : 1;    // [19]
            u32 display_plane_2_c_flip_pending_wait : 1;    // [20]
            u32 display_plane_1_c_vertical_blank_wait : 1;  // [21]
            u32 reserved_22 : 1;                            // [22]    MBZ
            u32 opcode : 6;                                 // [28:23] = 0x03
            u32 client : 3;                                 // [31:29] = 0x0 (MI_COMMAND)
        } __attribute__((packed));
        u32 raw;
    };

    // --- MI_FLUSH_DW ---
    union MI_FLUSH_DW_DW0 {
        struct {
            u32 dword_len : 6;    // [5:0]
            u32 reserved_a : 2;   // [7:6]   MBZ
            u32 notify_en : 1;    // [8]
            u32 flush_llc : 1;    // [9]
            u32 reserved_b : 4;   // [13:10] MBZ
            u32 post_sync : 2;    // [15:14] 2-Bit
            u32 reserved_c : 2;   // [17:16]
            u32 tlb_inv : 1;      // [18]
            u32 reserved_d : 2;   // [20:19] MBZ
            u32 store_index : 1;  // [21]
            u32 reserved_e : 1;   // [22]
            u32 opcode : 6;       // [28:23]
            u32 client : 3;       // [31:29]
        } __attribute__((packed));
        u32 raw;
    };

    union MI_FLUSH_DW_DW1 {
        struct {
            u32 reserved_a : 2;  // [1:0]   MBZ
            u32 addr_lo : 30;    // [31:2]
        } __attribute__((packed));
        u32 raw;
    };

    union MI_FLUSH_DW_DW2 {
        struct {
            u32 addr_hi : 16;     // [15:0]
            u32 reserved_a : 16;  // [31:16] MBZ
        } __attribute__((packed));
        u32 raw;
    };

    struct MI_FLUSH_DW_CMD {
        MI_FLUSH_DW_DW0 dw0;
        MI_FLUSH_DW_DW1 dw1;
        MI_FLUSH_DW_DW2 dw2;
        u64 immediate_data;
    } __attribute__((packed));

} // namespace gpu::intel::core

#endif // VESPERAOS_MI_COMMANDS_H