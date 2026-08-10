// cmd_3dstate_raster.h
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

#ifndef VESPERAOS_CMD_3DSTATE_RASTER_H
#define VESPERAOS_CMD_3DSTATE_RASTER_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_RASTER command (5 DWords).
 *
 * Configures the rasterizer stage in the 3D pipeline, including cull mode,
 * front winding, fill modes, depth offsets, and multisampling options.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 168-172 (3DSTATE_RASTER)
 */
union STATE_RASTER {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_RASTER = 0x50 };

    enum FillMode : u32 {
        FILL_SOLID     = 0x0,
        FILL_WIREFRAME = 0x1,
        FILL_POINT     = 0x2,
    };

    enum CullMode : u32 {
        CULL_BOTH  = 0x0,
        CULL_NONE  = 0x1,
        CULL_FRONT = 0x2,
        CULL_BACK  = 0x3,
    };

    enum FrontWinding : u32 {
        FRONTWINDING_CW  = 0x0,
        FRONTWINDING_CCW = 0x1,
    };

    enum ApiMode : u32 {
        API_DX9_OGL     = 0x0,
        API_DX10_0      = 0x1,
        API_DX10_1_PLUS = 0x2,
    };

    enum ForcedSampleCount : u32 {
        NUMRASTSAMPLES_0  = 0x0,
        NUMRASTSAMPLES_1  = 0x1,
        NUMRASTSAMPLES_2  = 0x2,
        NUMRASTSAMPLES_4  = 0x3,
        NUMRASTSAMPLES_8  = 0x4,
        NUMRASTSAMPLES_16 = 0x5,
    };

    enum DxMultisampleRasterMode : u32 {
        MSRASTMODE_OFF_PIXEL   = 0x0,
        MSRASTMODE_OFF_PATTERN = 0x1,
        MSRASTMODE_ON_PIXEL    = 0x2,
        MSRASTMODE_ON_PATTERN  = 0x3,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x3 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x50 (3DSTATE_RASTER)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 viewport_z_near_clip_test_enable : 1; ///< [0]     Viewport Z Near clip test
        u32 scissor_rectangle_enable : 1;         ///< [1]     Enable Scissor Rectangle
        u32 antialiasing_enable : 1;              ///< [2]     Alpha-based line antialiasing
        u32 back_face_fill_mode : 2;              ///< [4:3]   FillMode enum
        u32 front_face_fill_mode : 2;             ///< [6:5]   FillMode enum
        u32 global_depth_offset_enable_point : 1;    ///< [7]     Depth offset for point primitives
        u32 global_depth_offset_enable_wireframe : 1;///< [8]     Depth offset for wireframe primitives
        u32 global_depth_offset_enable_solid : 1;    ///< [9]     Depth offset for solid primitives
        u32 dx_multisample_rasterization_mode : 2;   ///< [11:10] DxMultisampleRasterMode enum
        u32 dx_multisample_rasterization_enable : 1; ///< [12]    DX Multisample rasterization enable
        u32 smooth_point_enable : 1;              ///< [13]    OGL Smooth point rasterization
        u32 force_multisampling : 1;              ///< [14]    0=Normal, 1=Force
        u32 reserved1_15 : 1;                     ///< [15]    MBZ
        u32 cull_mode : 2;                        ///< [17:16] CullMode enum
        u32 forced_sample_count : 3;              ///< [20:18] ForcedSampleCount enum
        u32 front_winding : 1;                    ///< [21]    FrontWinding enum
        u32 api_mode : 2;                         ///< [23:22] ApiMode enum
        u32 conservative_rasterization_enable : 1;///< [24]    Enable conservative rasterization
        u32 reserved1_25 : 1;                     ///< [25]    MBZ
        u32 viewport_z_far_clip_test_enable : 1;  ///< [26]    Viewport Z Far clip test
        u32 reserved1_27 : 1;                     ///< [27]    MBZ
        u32 reserved1_28 : 4;                     ///< [31:28] MBZ

        // DWord 2
        f32 global_depth_offset_constant;          ///< [31:0]  Constant term for depth offset

        // DWord 3
        f32 global_depth_offset_scale;             ///< [31:0]  Scale term for depth offset

        // DWord 4
        f32 global_depth_offset_clamp;             ///< [31:0]  Clamp term for depth offset
    } __attribute__((packed));

    u32 raw[5];

    /**
     * @brief Creates a default-initialized 3DSTATE_RASTER command.
     */
    [[nodiscard]] static constexpr STATE_RASTER create() {
        STATE_RASTER cmd{};
        cmd.dword_length = 0x3; // 5 DWords total - 2 = 3
        cmd.sub_opcode   = SUBOP_3DSTATE_RASTER;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_RASTER command configured for standard 3D rendering.
     *
     * Enables Z near/far clip testing, sets solid fill mode, and configures culling.
     *
     * @param cull Cull mode for triangles (default: CULL_NONE).
     * @param winding Front face winding order (default: FRONTWINDING_CCW).
     */
    [[nodiscard]] static constexpr STATE_RASTER create_default(CullMode cull = CULL_NONE,
                                                              FrontWinding winding = FRONTWINDING_CCW) {
        STATE_RASTER cmd = create();
        cmd.viewport_z_near_clip_test_enable = 1;
        cmd.viewport_z_far_clip_test_enable = 1;
        cmd.front_face_fill_mode = FILL_SOLID;
        cmd.back_face_fill_mode = FILL_SOLID;
        cmd.cull_mode = cull;
        cmd.front_winding = winding;
        cmd.api_mode = API_DX9_OGL;
        return cmd;
    }
};

static_assert(sizeof(STATE_RASTER) == 20, "STATE_RASTER must be 5 DWords (20 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_RASTER_H