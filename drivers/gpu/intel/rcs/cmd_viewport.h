// cmd_viewport.h
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

#ifndef VESPERAOS_CMD_VIEWPORT_H
#define VESPERAOS_CMD_VIEWPORT_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief SF_CLIP_VIEWPORT structure (16 DWords / 512 bits).
 *
 * Holds the NDC-to-screen-space viewport transform matrix plus guardband
 * clip extents. Lives in memory (Dynamic State Base Address-relative), NOT
 * in the command stream — 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP only
 * points at it.
 *
 * Viewport transform (standard hardware mapping, confirmed against the
 * PRM's field layout — the exact per-axis scale/translation formula itself
 * is not spelled out in the KBL PRM structure page, but matches the
 * well-known NDC-to-window mapping used across Intel Gen hardware):
 *   x_win = x_ndc * m00 + m30   (m00 = width/2,  m30 = x + width/2)
 *   y_win = y_ndc * m11 + m31   (m11 = height/2, m31 = y + height/2)
 *   z_win = z_ndc * m22 + m32   (m22 = max_depth - min_depth, m32 = min_depth)
 *
 * @note Must be 64-byte aligned in memory (per the pointer command's
 *       DynamicStateOffset[31:6] granularity).
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 552-553 (SF_CLIP_VIEWPORT)
 */
struct SF_CLIP_VIEWPORT {
    float m00;                  ///< DW0  Viewport Matrix Element m00 (X scale)
    float m11;                  ///< DW1  Viewport Matrix Element m11 (Y scale)
    float m22;                  ///< DW2  Viewport Matrix Element m22 (Z scale)
    float m30;                  ///< DW3  Viewport Matrix Element m30 (X translation)
    float m31;                  ///< DW4  Viewport Matrix Element m31 (Y translation)
    float m32;                  ///< DW5  Viewport Matrix Element m32 (Z translation)
    u32 reserved6;               ///< DW6  MBZ
    u32 reserved7;               ///< DW7  MBZ
    float x_min_clip_guardband;  ///< DW8  X Min Clip Guardband (normalized, Viewport.XMin == -1.0f)
    float x_max_clip_guardband;  ///< DW9  X Max Clip Guardband (normalized, Viewport.XMax == 1.0f)
    float y_min_clip_guardband;  ///< DW10 Y Min Clip Guardband (normalized, Viewport.YMin == -1.0f)
    float y_max_clip_guardband;  ///< DW11 Y Max Clip Guardband (normalized, Viewport.YMax == 1.0f)
    float x_min_viewport;        ///< DW12 X Min ViewPort, screen-space (NOT normalized)
    float x_max_viewport;        ///< DW13 X Max ViewPort, screen-space (NOT normalized)
    float y_min_viewport;        ///< DW14 Y Min ViewPort, screen-space (NOT normalized)
    float y_max_viewport;        ///< DW15 Y Max ViewPort, screen-space (NOT normalized)

    /**
     * @brief Builds a full-screen SF_CLIP_VIEWPORT for the given resolution.
     *
     * Guardband is set to exactly the NDC extents (-1..1) — no extra
     * headroom for off-screen clipping tolerance, which is fine for a
     * simple first triangle entirely inside the viewport.
     *
     * @param x, y            Screen-space origin of the viewport (usually 0, 0).
     * @param width, height   Viewport size in pixels.
     * @param min_depth       Minimum depth (PRM: must be >= 0.0, not -0.0).
     * @param max_depth       Maximum depth (PRM: must be in [0.0, 1.0]).
     */
    [[nodiscard]] static SF_CLIP_VIEWPORT create_full_screen(
        float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f
    ) {
        SF_CLIP_VIEWPORT vp{};
        vp.m00 = width * 0.5f;
        vp.m30 = x + width * 0.5f;
        vp.m11 = height * 0.5f;
        vp.m31 = y + height * 0.5f;
        vp.m22 = max_depth - min_depth;
        vp.m32 = min_depth;
        vp.reserved6 = 0;
        vp.reserved7 = 0;
        vp.x_min_clip_guardband = -1.0f;
        vp.x_max_clip_guardband = 1.0f;
        vp.y_min_clip_guardband = -1.0f;
        vp.y_max_clip_guardband = 1.0f;
        vp.x_min_viewport = x;
        vp.x_max_viewport = x + width;
        vp.y_min_viewport = y;
        vp.y_max_viewport = y + height;
        return vp;
    }
};

static_assert(sizeof(SF_CLIP_VIEWPORT) == 64, "SF_CLIP_VIEWPORT must be exactly 16 DWords (64 bytes)");

/**
 * @brief 3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP command.
 *
 * Points at an SF_CLIP_VIEWPORT structure via a 64-byte-aligned offset
 * relative to Dynamic State Base Address (set in STATE_BASE_ADDRESS).
 *
 * @see IHD-OS-KBL-Vol 2a (3DSTATE_VIEWPORT_STATE_POINTERS_SF_CLIP)
 */
union VIEWPORT_POINTERS_SF_CLIP {
    enum CommandSubOpcode : u32 { SUBOP_VIEWPORT_STATE_POINTERS_SF_CLIP = 0x21 };

    struct {
        // DWord 0
        u32 dword_length : 8;   ///< [7:0]   Default: 0x0
        u32 reserved0_8 : 8;    ///< [15:8]  MBZ
        u32 sub_opcode : 8;     ///< [23:16] Default: 0x21
        u32 opcode : 3;         ///< [26:24] Default: 0x0
        u32 sub_type : 2;       ///< [28:27] Default: 0x3
        u32 command_type : 3;   ///< [31:29] Default: 0x3

        // DWord 1
        u32 reserved1_0 : 6;              ///< [5:0]   MBZ
        u32 sf_clip_viewport_pointer : 26; ///< [31:6]  DynamicStateOffset[31:6], 64-byte aligned
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates the pointer command.
     *
     * @param dynamic_state_relative_offset  Byte offset of the
     *        SF_CLIP_VIEWPORT structure relative to Dynamic State Base
     *        Address, must be 64-byte aligned.
     */
    [[nodiscard]] static constexpr VIEWPORT_POINTERS_SF_CLIP create(u32 dynamic_state_relative_offset) {
        VIEWPORT_POINTERS_SF_CLIP cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_VIEWPORT_STATE_POINTERS_SF_CLIP;
        cmd.dword_length = 0x0;
        cmd.sf_clip_viewport_pointer = dynamic_state_relative_offset >> 6;
        return cmd;
    }
};

static_assert(sizeof(VIEWPORT_POINTERS_SF_CLIP) == 8, "VIEWPORT_POINTERS_SF_CLIP must be 2 DWords");

/**
 * @brief CC_VIEWPORT structure (2 DWords / 64 bits).
 *
 * Just the depth clamp range used by the Color Calculator stage. Also
 * memory-resident (Dynamic State Base Address-relative), 32-byte aligned.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, p.86 (CC_VIEWPORT)
 */
struct CC_VIEWPORT {
    float min_depth;  ///< DW0 Minimum Depth — must be >= 0.0, never NaN or -0.0
    float max_depth;  ///< DW1 Maximum Depth — must be in [0.0, 1.0], never NaN, >= min_depth

    [[nodiscard]] static constexpr CC_VIEWPORT create(float min_depth = 0.0f, float max_depth = 1.0f) {
        return CC_VIEWPORT{min_depth, max_depth};
    }
};

static_assert(sizeof(CC_VIEWPORT) == 8, "CC_VIEWPORT must be exactly 2 DWords (8 bytes)");

/**
 * @brief 3DSTATE_VIEWPORT_STATE_POINTERS_CC command.
 *
 * Points at a CC_VIEWPORT structure via a 32-byte-aligned offset relative
 * to Dynamic State Base Address.
 *
 * @see IHD-OS-KBL-Vol 2a (3DSTATE_VIEWPORT_STATE_POINTERS_CC)
 */
union VIEWPORT_POINTERS_CC {
    enum CommandSubOpcode : u32 { SUBOP_VIEWPORT_STATE_POINTERS_CC = 0x23 };

    struct {
        // DWord 0
        u32 dword_length : 8;   ///< [7:0]   Default: 0x0
        u32 reserved0_8 : 8;    ///< [15:8]  MBZ
        u32 sub_opcode : 8;     ///< [23:16] Default: 0x23
        u32 opcode : 3;         ///< [26:24] Default: 0x0
        u32 sub_type : 2;       ///< [28:27] Default: 0x3
        u32 command_type : 3;   ///< [31:29] Default: 0x3

        // DWord 1
        u32 reserved1_0 : 5;         ///< [4:0]   MBZ
        u32 cc_viewport_pointer : 27; ///< [31:5]  DynamicStateOffset[31:5], 32-byte aligned
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates the pointer command.
     *
     * @param dynamic_state_relative_offset  Byte offset of the CC_VIEWPORT
     *        structure relative to Dynamic State Base Address, must be
     *        32-byte aligned.
     */
    [[nodiscard]] static constexpr VIEWPORT_POINTERS_CC create(u32 dynamic_state_relative_offset) {
        VIEWPORT_POINTERS_CC cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_VIEWPORT_STATE_POINTERS_CC;
        cmd.dword_length = 0x0;
        cmd.cc_viewport_pointer = dynamic_state_relative_offset >> 5;
        return cmd;
    }
};

static_assert(sizeof(VIEWPORT_POINTERS_CC) == 8, "VIEWPORT_POINTERS_CC must be 2 DWords");

#endif  // VESPERAOS_CMD_VIEWPORT_H
