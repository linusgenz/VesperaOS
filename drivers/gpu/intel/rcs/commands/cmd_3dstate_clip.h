// cmd_3dstate_clip.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_CLIP_H
#define VESPERAOS_CMD_3DSTATE_CLIP_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_CLIP command (4 DWords).
 *
 * Configures the Fixed Function Clipper (CLIP) stage in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 36-41 (3DSTATE_CLIP)
 */
union STATE_CLIP {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_CLIP = 0x12 };

    enum ClipMode : u32 {
        CLIPMODE_NORMAL     = 0x0,
        CLIPMODE_REJECT_ALL = 0x3,
        CLIPMODE_ACCEPT_ALL = 0x4,
    };

    enum APIMode : u32 {
        API_OGL = 0x0,
        API_DX  = 0x1,
    };

    enum VertexSubPixelPrecision : u32 {
        SUBPIXEL_8_BIT = 0x0,
        SUBPIXEL_4_BIT = 0x1,
    };

    enum ProvokingVertex : u32 {
        PROVOKING_VERTEX_0 = 0x0,
        PROVOKING_VERTEX_1 = 0x1,
        PROVOKING_VERTEX_2 = 0x2,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x2 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x12 (3DSTATE_CLIP)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 user_clip_distance_cull_test_enable_bitmask : 8; ///< [7:0]   8-bit bitmask for DX10 cull distances
        u32 reserved1_8 : 2;                                 ///< [9:8]   MBZ
        u32 clipper_statistics_enable : 1;                   ///< [10]    Controls CL_INVOCATIONS_COUNT
        u32 reserved1_11 : 5;                                ///< [15:11] MBZ
        u32 force_clip_mode : 1;                             ///< [16]    Workaround override for SOL_INT::Render_Enable
        u32 force_user_clip_distance_clip_test_enable_bitmask : 1; ///< [17] Workaround override
        u32 cull_enable : 1;                                 ///< [18]    Enable EarlyCull function
        u32 vertex_sub_pixel_precision_select : 1;           ///< [19]    VertexSubPixelPrecision enum
        u32 force_user_clip_distance_cull_test_enable_bitmask : 1; ///< [20] Workaround override
        u32 reserved1_21 : 11;                               ///< [31:21] MBZ

        // DWord 2
        u32 triangle_strip_list_provoking_vertex_select : 2; ///< [1:0]   ProvokingVertex enum
        u32 line_strip_list_provoking_vertex_select : 2;     ///< [3:2]   ProvokingVertex enum
        u32 triangle_fan_provoking_vertex_select : 2;        ///< [5:4]   ProvokingVertex enum
        u32 reserved2_6 : 2;                                 ///< [7:6]   MBZ
        u32 non_perspective_barycentric_enable : 1;          ///< [8]     Required if non-perspective interpolation is used
        u32 perspective_divide_disable : 1;                  ///< [9]     Disables perspective divide (pre-transformed coords)
        u32 clip_mode : 3;                                   ///< [12:10] ClipMode enum
        u32 reserved2_13 : 3;                                ///< [15:13] MBZ
        u32 user_clip_distance_clip_test_enable_bitmask : 8; ///< [23:16] 8-bit bitmask for DX10 clip distances
        u32 reserved2_24 : 2;                                ///< [25:24] MBZ
        u32 guardband_clip_test_enable : 1;                  ///< [26]    Guardband X,Y extents clip test
        u32 reserved2_27 : 1;                                ///< [27]    MBZ
        u32 viewport_xy_clip_test_enable : 1;                ///< [28]    Viewport X,Y extents [-1,1] clip test
        u32 reserved2_29 : 1;                                ///< [29]    MBZ
        u32 api_mode : 1;                                    ///< [30]    APIMode enum (0 = OGL, 1 = DX)
        u32 clip_enable : 1;                                 ///< [31]    1 = Enabled, 0 = Disabled (pass-through)

        // DWord 3
        u32 maximum_vp_index : 4;         ///< [3:0]   U4-1; Max active viewports index
        u32 reserved3_4 : 1;              ///< [4]     MBZ
        u32 force_zero_rta_index_enable : 1; ///< [5]     1 = Force Render Target Array Index to 0
        u32 maximum_point_width : 11;     ///< [16:6]  U8.3 pixels
        u32 minimum_point_width : 11;     ///< [27:17] U8.3 pixels
        u32 reserved3_28 : 4;             ///< [31:28] MBZ
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a default-initialized 3DSTATE_CLIP command.
     */
    [[nodiscard]] static constexpr STATE_CLIP create() {
        STATE_CLIP cmd{};
        cmd.dword_length = 0x2; // 4 DWords total - 2 = 2
        cmd.sub_opcode   = SUBOP_3DSTATE_CLIP;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CLIP command configured for standard 3D rendering.
     *
     * Enables clipping, sets Viewport XY clip test, Guardband clip test, and API mode.
     */
    [[nodiscard]] static constexpr STATE_CLIP create_default() {
        STATE_CLIP cmd = create();
        cmd.clip_enable = 1;
        cmd.viewport_xy_clip_test_enable = 1;
        cmd.guardband_clip_test_enable = 1;
        cmd.clip_mode = CLIPMODE_NORMAL;
        cmd.api_mode = API_OGL;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CLIP command that disables the Clipper stage (pass-through).
     */
    [[nodiscard]] static constexpr STATE_CLIP create_disabled() {
        STATE_CLIP cmd = create();
        cmd.clip_enable = 0; // Disable Clipper stage
        return cmd;
    }
};

static_assert(sizeof(STATE_CLIP) == 16, "STATE_CLIP must be 4 DWords (16 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_CLIP_H