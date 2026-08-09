// cmd_3dstate_wm.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 08.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_WM_H
#define VESPERAOS_CMD_3DSTATE_WM_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_WM command (2 DWords).
 *
 * Configures the Windower (WM) fixed-function stage: early depth/stencil control,
 * barycentric interpolation enables, position ZW interpolation mode, line/point
 * rasterization rules, and legacy depth buffer operations.
 * Lives directly in the command stream.
 *
 * IMPORTANT — C++ bitfield declaration order determines actual bit
 * placement, NOT the doc comments. Every field below is declared
 * LSB-first within its DWord to match the PRM's bit numbering.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 271-276 (3DSTATE_WM)
 */
union STATE_WM {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_WM = 0x14 };

    enum ForceKillPixel : u32 {
        KILL_NORMAL    = 0x0,  ///< Pixel Shader Kill Pixel is computed normally
        KILL_FORCE_OFF = 0x1,  ///< Forces Pixel Shader Kill Pixel Off
        KILL_FORCE_ON  = 0x2,  ///< Forces Pixel Shader Kill Pixel On
    };

    enum PointRasterizationRule : u32 {
        RASTRULE_UPPER_LEFT  = 0x0,  ///< Normal upper-left rules for surface primitives
        RASTRULE_UPPER_RIGHT = 0x1,  ///< OpenGL point rasterization rules (upper right)
    };

    enum AARegionWidth : u32 {
        AA_WIDTH_0_5 = 0x0,  ///< 0.5 pixels
        AA_WIDTH_1_0 = 0x1,  ///< 1.0 pixels
        AA_WIDTH_2_0 = 0x2,  ///< 2.0 pixels
        AA_WIDTH_4_0 = 0x3,  ///< 4.0 pixels
    };

    enum BarycentricMask : u32 {
        BARY_PERSPECTIVE_PIXEL    = 1 << 0,  ///< Perspective Pixel Location required
        BARY_PERSPECTIVE_CENTROID = 1 << 1,  ///< Perspective Centroid required
        BARY_PERSPECTIVE_SAMPLE   = 1 << 2,  ///< Perspective Sample required
        BARY_NONPERSPECTIVE_PIXEL = 1 << 3,  ///< Non-perspective Pixel Location required
        BARY_NONPERSPECTIVE_CENTROID = 1 << 4, ///< Non-perspective Centroid required
        BARY_NONPERSPECTIVE_SAMPLE   = 1 << 5, ///< Non-perspective Sample required
    };

    enum PositionZWInterpolationMode : u32 {
        INTERP_PIXEL    = 0x0,  ///< Evaluate Z & W at pixel center or UL corner
        INTERP_CENTROID = 0x2,  ///< Evaluate Z & W at centroid
        INTERP_SAMPLE   = 0x3,  ///< Evaluate Z & W at sample (requires MSDISPMODE_PERSAMPLE)
    };

    enum ForceThreadDispatch : u32 {
        DISPATCH_NORMAL    = 0x0,  ///< Thread Dispatch Enable is computed normally
        DISPATCH_FORCE_OFF = 0x1,  ///< Forces Thread Dispatch Enable Off
        DISPATCH_FORCE_ON  = 0x2,  ///< Forces Thread Dispatch Enable On
    };

    enum EarlyDepthStencilControl : u32 {
        EARLY_DS_NORMAL = 0x0,  ///< Depth/Stencil Test/Write behaves post-shader (legacy)
        EARLY_DS_PSEXEC = 0x1,  ///< Post-shader behavior, executes PS even on test failure
        EARLY_DS_PREPS  = 0x2,  ///< Early Depth/Stencil test pre-shader
    };

    struct {
        // ====================================================================
        // DWord 0
        // ====================================================================
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0, Total Length - 2
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x14 (3DSTATE_WM)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // ====================================================================
        // DWord 1
        // ====================================================================
        u32 force_kill_pixel_enable : 2;                   ///< [1:0]   ForceKillPixel enum (Default: Normal)
        u32 point_rasterization_rule : 1;                  ///< [2]     PointRasterizationRule enum
        u32 line_stipple_enable : 1;                       ///< [3]     1 = Enables Line Stipple function
        u32 polygon_stipple_enable : 1;                    ///< [4]     1 = Enables Polygon Stipple function
        u32 reserved1_5 : 1;                               ///< [5]     MBZ
        u32 line_antialiasing_region_width : 2;            ///< [7:6]   AARegionWidth enum
        u32 line_end_cap_antialiasing_region_width : 2;    ///< [9:8]   AARegionWidth enum
        u32 reserved1_10 : 1;                              ///< [10]    MBZ
        u32 barycentric_interpolation_mode : 6;            ///< [16:11] BarycentricMask bitfield
        u32 position_zw_interpolation_mode : 2;            ///< [18:17] PositionZWInterpolationMode enum
        u32 force_thread_dispatch_enable : 2;              ///< [20:19] ForceThreadDispatch enum (Default: Normal)
        u32 early_depth_stencil_control : 2;               ///< [22:21] EarlyDepthStencilControl enum
        u32 reserved1_23 : 3;                              ///< [25:23] MBZ
        u32 legacy_diamond_line_rasterization : 1;         ///< [26]    1 = DX9 rules, 0 = DX10 rules
        u32 legacy_hierarchical_depth_buffer_resolve_enable : 1; ///< [27] 1 = Resolve HiZ to depth buffer
        u32 legacy_depth_buffer_resolve_enable : 1;        ///< [28]    1 = Resolve depth buffer to HiZ
        u32 reserved1_29 : 1;                              ///< [29]    MBZ
        u32 legacy_depth_buffer_clear_enable : 1;          ///< [30]    1 = Initialize depth buffer on rendering
        u32 statistics_enable : 1;                         ///< [31]    1 = Gather Windower & pixel statistics
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_WM command.
     */
    [[nodiscard]] static constexpr STATE_WM create() {
        STATE_WM cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = SUBOP_3DSTATE_WM;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }
};

static_assert(sizeof(STATE_WM) == 8, "STATE_WM must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_WM_H