// cmd_scissor.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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

#ifndef VESPERAOS_CMD_SCISSOR_H
#define VESPERAOS_CMD_SCISSOR_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief SCISSOR_RECT structure (2 DWords / 64 bits).
 *
 * Memory-resident (Dynamic State Base Address-relative, 32-byte aligned),
 * NOT part of the command stream — 3DSTATE_SCISSOR_STATE_POINTERS only
 * points at it. Coordinates are relative to the Drawing Rectangle origin
 * (see cmd_drawing_rectangle.h), not the raw color buffer.
 *
 * @note If Y Min > Y Max (or X Min > X Max), ALL primitives are discarded
 *       for this viewport — that is how "no scissor" would go wrong if
 *       min/max were swapped.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, p.550 (SCISSOR_RECT)
 */
struct SCISSOR_RECT {
    u32 x_min : 16;  ///< DW0 [15:0]  Scissor Rectangle X Min (inclusive)
    u32 y_min : 16;  ///< DW0 [31:16] Scissor Rectangle Y Min (inclusive)
    u32 x_max : 16;  ///< DW1 [15:0]  Scissor Rectangle X Max (inclusive)
    u32 y_max : 16;  ///< DW1 [31:16] Scissor Rectangle Y Max (inclusive)

    /**
     * @brief Creates a scissor rect covering the full drawing rectangle —
     *        i.e. effectively "no scissoring" for a first triangle that's
     *        already guaranteed to be fully on-screen.
     *
     * @param width, height  Size of the drawing rectangle in pixels.
     */
    [[nodiscard]] static constexpr SCISSOR_RECT create_full(u32 width, u32 height) {
        SCISSOR_RECT r{};
        r.x_min = 0;
        r.y_min = 0;
        r.x_max = width - 1;   // inclusive max
        r.y_max = height - 1;  // inclusive max
        return r;
    }
};

static_assert(sizeof(SCISSOR_RECT) == 8, "SCISSOR_RECT must be exactly 2 DWords (8 bytes)");

/**
 * @brief 3DSTATE_SCISSOR_STATE_POINTERS command.
 *
 * Points at a SCISSOR_RECT structure via a 32-byte-aligned offset relative
 * to Dynamic State Base Address.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, p.205 (3DSTATE_SCISSOR_STATE_POINTERS)
 */
union SCISSOR_STATE_POINTERS {
    enum CommandSubOpcode : u32 { SUBOP_SCISSOR_STATE_POINTERS = 0x0F };

    struct {
        // DWord 0
        u32 dword_length : 8;   ///< [7:0]   Default: 0x0
        u32 reserved0_8 : 8;    ///< [15:8]  MBZ
        u32 sub_opcode : 8;     ///< [23:16] Default: 0x0F
        u32 opcode : 3;         ///< [26:24] Default: 0x0
        u32 sub_type : 2;       ///< [28:27] Default: 0x3
        u32 command_type : 3;   ///< [31:29] Default: 0x3

        // DWord 1
        u32 reserved1_0 : 5;          ///< [4:0]   MBZ
        u32 scissor_rect_pointer : 27; ///< [31:5]  DynamicStateOffset[31:5], 32-byte aligned
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates the pointer command.
     *
     * @param dynamic_state_relative_offset  Byte offset of the SCISSOR_RECT
     *        structure relative to Dynamic State Base Address, must be
     *        32-byte aligned.
     */
    [[nodiscard]] static constexpr SCISSOR_STATE_POINTERS create(u32 dynamic_state_relative_offset) {
        SCISSOR_STATE_POINTERS cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_SCISSOR_STATE_POINTERS;
        cmd.dword_length = 0x0;
        cmd.scissor_rect_pointer = dynamic_state_relative_offset >> 5;
        return cmd;
    }
};

static_assert(sizeof(SCISSOR_STATE_POINTERS) == 8, "SCISSOR_STATE_POINTERS must be 2 DWords");

#endif  // VESPERAOS_CMD_SCISSOR_H
