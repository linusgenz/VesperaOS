// blt_commands.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 08.03.26.
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
#ifndef VESPERAOS_BLT_COMMANDS_H
#define VESPERAOS_BLT_COMMANDS_H

#include <vespera/types.h>

// ReSharper disable CppInconsistentNaming

namespace blt {

    // Shared enumerations

    /// Color depth encoding for BR13 [25:24] in classic 2D BLT commands.
    enum BLT_COLOR_DEPTH : u32 {
        COLOR_DEPTH_8BPP = 0b00,
        COLOR_DEPTH_16BPP = 0b01,
        COLOR_DEPTH_16BPP_2 = 0b10,
        COLOR_DEPTH_32BPP = 0b11,
    };

    /// Color depth encoding for BR13 [26:24] in XY_FAST_COPY_BLT (3-bit field).
    enum BLT_FAST_COLOR_DEPTH : u32 {
        FAST_COLOR_DEPTH_8BPP = 0b000,
        FAST_COLOR_DEPTH_16BPP = 0b001,
        FAST_COLOR_DEPTH_32BPP = 0b011,
        FAST_COLOR_DEPTH_64BPP = 0b100,   // 64KB tiling only
        FAST_COLOR_DEPTH_128BPP = 0b101,  // 64KB tiling only
    };

    /// Tiling method for XY_FAST_COPY_BLT DW0: src [21:20], dst [14:13].
    enum BLT_TILING_METHOD : u32 {
        TILING_LINEAR = 0b00,
        TILING_X = 0b01,
        TILING_Y = 0b10,  // Legacy Tile-Y or 4K Tile-YF (see DW1)
        TILING_64K = 0b11,
    };

    /// Command client IDs for BR00 [31:29].
    enum BLT_CLIENT : u32 {
        CLIENT_MI = 0x0,            // Memory Interface (MI_* commands)
        CLIENT_2D_PROCESSOR = 0x2,  // 2D Blitter engine (XY_* commands)
    };

    /// Opcode values for BR00. See BLT Commands Reference for field widths per client.
    enum BLT_OPCODE : u32 {
        OPCODE_MI_NOOP = 0x00,
        OPCODE_MI_FLUSH_DW = 0x26,
        OPCODE_XY_COLOR_BLT = 0x50,
        OPCODE_XY_SRC_COPY_BLT = 0x53,
        OPCODE_XY_MONO_SRC_COPY_BLT = 0x54,
        OPCODE_XY_FAST_COPY_BLT = 0x42,
    };

    // Shared coordinate registers

    /// BR22: top-left coordinate — [31:16] Y1, [15:0] X1.
    union BLT_BR22 {
        struct {
            u32 x1 : 16;
            u32 y1 : 16;
        } __attribute__((packed));
        u32 d_word;
    };

    /// BR23: bottom-right coordinate — [31:16] Y2, [15:0] X2.
    union BLT_BR23 {
        struct {
            u32 x2 : 16;
            u32 y2 : 16;
        } __attribute__((packed));
        u32 d_word;
    };

    /// BR26: source top-left coordinate — [31:16] Y1, [15:0] X1.
    union BLT_BR26 {
        struct {
            u32 src_x1 : 16;
            u32 src_y1 : 16;
        } __attribute__((packed));
        u32 d_word;
    };

    // XY_COLOR_BLT  (opcode 0x50, 7 DWORDs)

    /// DW0 / BR00 for XY_COLOR_BLT.
    union XY_COLOR_BLT_DW0 {
        struct {
            u32 dword_len : 8;      // [7:0]   = 0x05
            u32 reserved0 : 3;      // [10:8]  MBZ
            u32 tiling_enable : 1;  // [11]
            u32 reserved1 : 8;      // [19:12] MBZ
            u32 write_rgb : 1;      // [20]
            u32 write_alpha : 1;    // [21]
            u32 opcode : 7;         // [28:22] = 0x50
            u32 client : 3;         // [31:29] = 0x2
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW1 / BR13 for XY_COLOR_BLT.
    union XY_COLOR_BLT_DW1 {
        struct {
            u32 dest_pitch : 16;  // [15:0]
            u32 rop : 8;          // [23:16]
            u32 color_depth : 2;  // [25:24] BLT_COLOR_DEPTH
            u32 reserved0 : 4;    // [29:26] MBZ
            u32 clipping : 1;     // [30]
            u32 reserved1 : 1;    // [31]    MBZ
        } __attribute__((packed));
        u32 d_word;
    };

    /// Full XY_COLOR_BLT command packet (7 DWORDs).
    struct XY_COLOR_BLT_CMD {
        XY_COLOR_BLT_DW0 dw0;
        XY_COLOR_BLT_DW1 dw1;
        BLT_BR22 dw2;          // Destination top-left
        BLT_BR23 dw3;          // Destination bottom-right
        u32 dest_addr_lo;
        u32 dest_addr_hi;
        u32 solid_color;       // BR16 — ARGB8888
    } __attribute__((packed));

    // XY_SRC_COPY_BLT  (opcode 0x53, 10 DWORDs)

    /// DW0 / BR00 for XY_SRC_COPY_BLT.
    union XY_SRC_COPY_BLT_DW0 {
        struct {
            u32 dword_len : 8;          // [7:0]   = 0x08
            u32 reserved0 : 3;          // [10:8]  MBZ
            u32 dst_tiling_enable : 1;  // [11]
            u32 reserved1 : 3;          // [14:12] MBZ
            u32 src_tiling_enable : 1;  // [15]
            u32 reserved2 : 4;          // [19:16] MBZ
            u32 write_rgb : 1;          // [20]
            u32 write_alpha : 1;        // [21]
            u32 opcode : 7;             // [28:22] = 0x53
            u32 client : 3;             // [31:29] = 0x2
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW1 / BR13 for XY_SRC_COPY_BLT. Identical layout to XY_COLOR_BLT_DW1.
    union XY_SRC_COPY_BLT_DW1 {
        struct {
            u32 dest_pitch : 16;  // [15:0]
            u32 rop : 8;          // [23:16]
            u32 color_depth : 2;  // [25:24] BLT_COLOR_DEPTH
            u32 reserved0 : 4;    // [29:26] MBZ
            u32 clipping : 1;     // [30]
            u32 reserved1 : 1;    // [31]    MBZ
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW7 / BR11 for XY_SRC_COPY_BLT — source pitch.
    union XY_SRC_COPY_BLT_DW7 {
        struct {
            u32 src_pitch : 16;  // [15:0]
            u32 reserved0 : 16;  // [31:16] MBZ
        } __attribute__((packed));
        u32 d_word;
    };

    /// Full XY_SRC_COPY_BLT command packet (10 DWORDs).
    struct XY_SRC_COPY_BLT_CMD {
        XY_SRC_COPY_BLT_DW0 dw0;
        XY_SRC_COPY_BLT_DW1 dw1;
        BLT_BR22 dw2;          // Destination top-left
        BLT_BR23 dw3;          // Destination bottom-right
        u32 dest_addr_lo;
        u32 dest_addr_hi;
        BLT_BR26 dw6;          // Source top-left
        XY_SRC_COPY_BLT_DW7 dw7;
        u32 src_addr_lo;
        u32 src_addr_hi;
    } __attribute__((packed));

    // XY_MONO_SRC_COPY_BLT  (opcode 0x54, 11 DWORDs)

    /// DW0 / BR00 for XY_MONO_SRC_COPY_BLT.
    union XY_MONO_SRC_COPY_BLT_DW0 {
        struct {
            u32 dword_len : 8;         // [7:0]   = 0x08
            u32 reserved0 : 3;         // [10:8]  MBZ
            u32 tiling_enable : 1;     // [11]
            u32 reserved1 : 5;         // [16:12] MBZ
            u32 mono_src_bit_pos : 3;  // [19:17] first pixel bit position
            u32 write_rgb : 1;         // [20]
            u32 write_alpha : 1;       // [21]
            u32 opcode : 7;            // [28:22] = 0x54
            u32 client : 3;            // [31:29] = 0x2
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW1 / BR13 for XY_MONO_SRC_COPY_BLT.
    union XY_MONO_SRC_COPY_BLT_DW1 {
        struct {
            u32 dest_pitch : 16;   // [15:0]
            u32 rop : 8;           // [23:16] must involve source
            u32 color_depth : 2;   // [25:24] BLT_COLOR_DEPTH
            u32 reserved0 : 3;     // [28:26] MBZ
            u32 transparency : 1;  // [29]    0 = bg color, 1 = transparent
            u32 clipping : 1;      // [30]
            u32 reserved1 : 1;     // [31]    MBZ
        } __attribute__((packed));
        u32 d_word;
    };

    /// Full XY_MONO_SRC_COPY_BLT command packet (11 DWORDs).
    struct XY_MONO_SRC_COPY_BLT_CMD {
        XY_MONO_SRC_COPY_BLT_DW0 dw0;
        XY_MONO_SRC_COPY_BLT_DW1 dw1;
        BLT_BR22 dw2;           // Destination top-left
        BLT_BR23 dw3;           // Destination bottom-right
        u32 dest_addr_lo;
        u32 dest_addr_hi;
        u32 mono_src_addr_lo;
        u32 mono_src_addr_hi;
        u32 bg_color;           // BR18
        u32 fg_color;           // BR19
        u32 trailing;           // DW10 = 0 (alignment pad)
    } __attribute__((packed));

    // XY_FAST_COPY_BLT  (opcode 0x42, 10 DWORDs)

    /// DW0 / BR00 for XY_FAST_COPY_BLT.
    union XY_FAST_COPY_BLT_DW0 {
        struct {
            u32 dword_len : 8;   // [7:0]   = 0x08
            u32 reserved0 : 5;   // [12:8]  MBZ
            u32 dst_tiling : 2;  // [14:13] BLT_TILING_METHOD
            u32 reserved1 : 5;   // [19:15] MBZ
            u32 src_tiling : 2;  // [21:20] BLT_TILING_METHOD
            u32 opcode : 7;      // [28:22] = 0x42
            u32 client : 3;      // [31:29] = 0x2
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW1 / BR13 for XY_FAST_COPY_BLT.
    union XY_FAST_COPY_BLT_DW1 {
        struct {
            u32 dest_pitch : 16;      // [15:0]  bytes (linear) or DWORDs (tiled)
            u32 reserved0 : 8;        // [23:16] MBZ
            u32 color_depth : 3;      // [26:24] BLT_FAST_COLOR_DEPTH
            u32 reserved1 : 3;        // [29:27] MBZ
            u32 tile_y_type_dst : 1;  // [30]    0 = Tile-Y, 1 = Tile-YF
            u32 tile_y_type_src : 1;  // [31]    0 = Tile-Y, 1 = Tile-YF
        } __attribute__((packed));
        u32 d_word;
    };

    /// DW7 / BR11 for XY_FAST_COPY_BLT — source pitch (same encoding as dest_pitch).
    union XY_FAST_COPY_BLT_DW7 {
        struct {
            u32 src_pitch : 16;  // [15:0]
            u32 reserved0 : 16;  // [31:16] MBZ
        } __attribute__((packed));
        u32 d_word;
    };

    /// Full XY_FAST_COPY_BLT command packet (10 DWORDs).
    struct XY_FAST_COPY_BLT_CMD {
        XY_FAST_COPY_BLT_DW0 dw0;
        XY_FAST_COPY_BLT_DW1 dw1;
        BLT_BR22 dw2;          // Destination top-left
        BLT_BR23 dw3;          // Destination bottom-right
        u32 dest_addr_lo;
        u32 dest_addr_hi;
        BLT_BR26 dw6;          // Source top-left
        XY_FAST_COPY_BLT_DW7 dw7;
        u32 src_addr_lo;
        u32 src_addr_hi;
    } __attribute__((packed));

    // MI_FLUSH_DW  (opcode 0x26, 5 DWORDs)

    /// DW0 for MI_FLUSH_DW.
    union MI_FLUSH_DW_DW0 {
        struct {
            u32 dword_len : 8;    // [7:0]   = 0x03
            u32 reserved0 : 6;    // [13:8]  MBZ
            u32 post_sync : 1;    // [14]    enable write to address_or_offset
            u32 reserved1 : 6;    // [20:15] MBZ
            u32 store_index : 1;  // [21]    1 = DW1 is HWSP DWORD index
            u32 reserved2 : 1;    // [22]    MBZ
            u32 opcode : 6;       // [28:23] = 0x26
            u32 client : 3;       // [31:29] = 0x0
        } __attribute__((packed));
        u32 d_word;
    };

    /// Full MI_FLUSH_DW command packet (5 DWORDs).
    struct MI_FLUSH_DW_CMD {
        MI_FLUSH_DW_DW0 dw0;
        u32 address_or_offset;  // DW1 — target address low or HWSP index
        u32 address_hi;         // DW2 — 0 when using HWSP index
        u32 immediate_data;     // DW3 — value to write (e.g. sequence number)
        u32 reserved;           // DW4 = 0
    } __attribute__((packed));

}  // namespace blt

#endif  // VESPERAOS_BLT_COMMANDS_H