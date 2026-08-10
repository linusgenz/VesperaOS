// cmd_3dstate_sf.h
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

#ifndef VESPERAOS_CMD_3DSTATE_SF_H
#define VESPERAOS_CMD_3DSTATE_SF_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_SF command (4 DWords).
 *
 * Configures the Strip & Fan / Setup Function (SF) stage in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 206-210 (3DSTATE_SF)
 */
union STATE_SF {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_SF = 0x13 };

    enum LineEndCapAARegionWidth : u32 {
        AA_REGION_0_5_PIXELS = 0x0,
        AA_REGION_1_0_PIXELS = 0x1,
        AA_REGION_2_0_PIXELS = 0x2,
        AA_REGION_4_0_PIXELS = 0x3,
    };

    enum ProvokingVertex : u32 {
        PROVOKING_VERTEX_0 = 0x0,
        PROVOKING_VERTEX_1 = 0x1,
        PROVOKING_VERTEX_2 = 0x2,
    };

    enum VertexSubPixelPrecision : u32 {
        SUBPIXEL_8_BIT = 0x0,
        SUBPIXEL_4_BIT = 0x1,
    };

    enum PointWidthSource : u32 {
        POINT_WIDTH_VERTEX = 0x0,
        POINT_WIDTH_STATE  = 0x1,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x2 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x13 (3DSTATE_SF)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 reserved1_0 : 1;                   ///< [0]     MBZ
        u32 viewport_transform_enable : 1;     ///< [1]     Controls Viewport Transform function
        u32 reserved1_2 : 8;                   ///< [9:2]   MBZ
        u32 statistics_enable : 1;             ///< [10]    Increments CL_PRIMITIVES_COUNT on behalf of CLIP
        u32 legacy_global_depth_bias_enable : 1; ///< [11]  Enables SF to use Global Depth Offset Constant unmodified
        u32 line_width : 18;                   ///< [29:12] U11.7; Controls width of line primitives
        u32 reserved1_30 : 2;                  ///< [31:30] MBZ

        // DWord 2
        u32 reserved2_0 : 16;                                 ///< [15:0]  MBZ
        u32 line_end_cap_antialiasing_region_width : 2;      ///< [17:16] LineEndCapAARegionWidth enum
        u32 reserved2_18 : 14;                                ///< [31:18] MBZ

        // DWord 3
        u32 point_width : 11;                             ///< [10:0]  U8.3; Size of point primitives [0.125, 255.875]
        u32 point_width_source : 1;                       ///< [11]    PointWidthSource enum
        u32 vertex_sub_pixel_precision_select : 1;       ///< [12]    VertexSubPixelPrecision enum
        u32 smooth_point_enable : 1;                      ///< [13]    Enables logic to draw smooth OGL Points
        u32 aa_line_distance_mode : 1;                    ///< [14]    0 = Legacy, 1 = True distance computation
        u32 reserved3_15 : 10;                            ///< [24:15] MBZ
        u32 triangle_fan_provoking_vertex_select : 2;    ///< [26:25] ProvokingVertex enum
        u32 line_strip_list_provoking_vertex_select : 2;  ///< [28:27] ProvokingVertex enum
        u32 triangle_strip_list_provoking_vertex_select : 2; ///< [30:29] ProvokingVertex enum
        u32 last_pixel_enable : 1;                        ///< [31]    If enabled, last pixel of diamond line is lit
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a default-initialized 3DSTATE_SF command.
     */
    [[nodiscard]] static constexpr STATE_SF create() {
        STATE_SF cmd{};
        cmd.dword_length = 0x2; // 4 DWords total - 2 = 2
        cmd.sub_opcode   = SUBOP_3DSTATE_SF;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_SF command configured with viewport transform enabled.
     */
    [[nodiscard]] static constexpr STATE_SF create_default() {
        STATE_SF cmd = create();
        cmd.viewport_transform_enable = 1;
        cmd.point_width_source = POINT_WIDTH_STATE;
        cmd.point_width = 0x8; // 1.0 pixel in U8.3 (1 << 3)
        return cmd;
    }
};

static_assert(sizeof(STATE_SF) == 16, "STATE_SF must be 4 DWords (16 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_SF_H