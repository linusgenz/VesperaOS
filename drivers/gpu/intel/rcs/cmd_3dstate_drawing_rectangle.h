// cmd_3dstate_drawing_rectangle.h
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

#ifndef VESPERAOS_CMD_DRAWING_RECTANGLE_H
#define VESPERAOS_CMD_DRAWING_RECTANGLE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_DRAWING_RECTANGLE command (4 DWords).
 *
 * Lives directly in the command stream. Sets the 3D drawing rectangle:
 * a clip rectangle against the Color (Destination) Buffer, plus an
 * origin used to map incoming (Draw Rectangle-relative) vertex
 * positions into Color Buffer space.
 *
 * @note Unlike the pipelined 3DSTATE_* commands (cmd_viewport.h,
 *       cmd_scissor.h), this is a 3DSTATE_NONPIPELINED command — the
 *       3D Command Opcode is 0x1, not 0x0.
 * @note If Y Min > Y Max (or X Min > X Max) for the clip rect, ALL
 *       primitives are discarded — same "no scissor gone wrong" trap as
 *       SCISSOR_RECT (see cmd_scissor.h).
 * @note Drawing Rectangle Origin X/Y are signed (S15), range
 *       [-16384, 16383], relative to the Color Buffer origin.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 61-63 (3DSTATE_DRAWING_RECTANGLE)
 */
union DRAWING_RECTANGLE {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_DRAWING_RECTANGLE = 0x00 };

    enum CoreModeSelect : u32 {
        CORE_MODE_LEGACY = 0x0,        ///< Both cores enabled, both updated
        CORE_MODE_CORE_0_ENABLED = 0x1, ///< State updated in Core 0 only
        CORE_MODE_CORE_1_ENABLED = 0x2, ///< State updated in Core 1 only
    };

    struct {
        // DWord 0
        u32 dword_length : 8;     ///< [7:0]   Default: 0x2, Total Length - 2
        u32 reserved0_8 : 6;      ///< [13:8]  MBZ
        u32 core_mode_select : 2; ///< [15:14] CoreModeSelect enum
        u32 sub_opcode : 8;       ///< [23:16] Default: 0x00
        u32 opcode : 3;           ///< [26:24] Default: 0x1 (3DSTATE_NONPIPELINED)
        u32 sub_type : 2;         ///< [28:27] Default: 0x3
        u32 command_type : 3;     ///< [31:29] Default: 0x3

        // DWord 1
        u32 clip_x_min : 16;  ///< [15:0]  U16, Clipped Drawing Rectangle X Min (inclusive)
        u32 clip_y_min : 16;  ///< [31:16] U16, Clipped Drawing Rectangle Y Min (inclusive)

        // DWord 2
        u32 clip_x_max : 16;  ///< [15:0]  U16, Clipped Drawing Rectangle X Max (inclusive)
        u32 clip_y_max : 16;  ///< [31:16] U16, Clipped Drawing Rectangle Y Max (inclusive)

        // DWord 3
        i32 origin_x : 16;  ///< [15:0]  S15, Drawing Rectangle Origin X, range [-16384,16383]
        i32 origin_y : 16;  ///< [31:16] S15, Drawing Rectangle Origin Y, range [-16384,16383]
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a drawing rectangle covering the full color buffer,
     *        with a zero origin — the common case for a first triangle
     *        that's already guaranteed to be fully on-screen.
     *
     * @param width, height  Size of the color buffer in pixels.
     */
    [[nodiscard]] static constexpr DRAWING_RECTANGLE create_full(u32 width, u32 height) {
        DRAWING_RECTANGLE cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_NONPIPELINED;
        cmd.sub_opcode = SUBOP_3DSTATE_DRAWING_RECTANGLE;
        cmd.dword_length = 0x2;
        cmd.core_mode_select = CORE_MODE_LEGACY;

        cmd.clip_x_min = 0;
        cmd.clip_y_min = 0;
        cmd.clip_x_max = width - 1;   // inclusive max
        cmd.clip_y_max = height - 1;  // inclusive max

        cmd.origin_x = 0;
        cmd.origin_y = 0;

        return cmd;
    }
};

static_assert(sizeof(DRAWING_RECTANGLE) == 16, "DRAWING_RECTANGLE must be exactly 4 DWords (16 bytes)");

#endif  // VESPERAOS_CMD_DRAWING_RECTANGLE_H