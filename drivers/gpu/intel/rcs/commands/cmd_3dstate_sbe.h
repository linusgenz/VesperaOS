// cmd_3dstate_sbe.h
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

#ifndef VESPERAOS_CMD_3DSTATE_SBE_H
#define VESPERAOS_CMD_3DSTATE_SBE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_SBE command (6 DWords).
 *
 * Configures the Setup Buffer Engine (SBE) stage in the 3D pipeline.
 * Handles attribute routing, active component formats, and point sprite origin.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 195-202 (3DSTATE_SBE)
 */
union STATE_SBE {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_SBE = 0x1F };

    enum PointSpriteTextureCoordinateOrigin : u32 {
        ORIGIN_UPPER_LEFT = 0x0,
        ORIGIN_LOWER_LEFT = 0x1,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x4 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x1F (3DSTATE_SBE)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 primitive_id_override_attribute_select : 5; ///< [4:0]   Attribute overridden with Primitive ID
        u32 vertex_urb_entry_read_offset : 6;          ///< [10:5]  Offset in 256-bit units
        u32 vertex_urb_entry_read_length : 5;          ///< [15:11] Read length in 256-bit register increments [1,16]
        u32 primitive_id_override_component_x : 1;     ///< [16]    Override X with Primitive ID
        u32 primitive_id_override_component_y : 1;     ///< [17]    Override Y with Primitive ID
        u32 primitive_id_override_component_z : 1;     ///< [18]    Override Z with Primitive ID
        u32 primitive_id_override_component_w : 1;     ///< [19]    Override W with Primitive ID
        u32 point_sprite_texture_coordinate_origin : 1;///< [20]    PointSpriteTextureCoordinateOrigin enum
        u32 attribute_swizzle_enable : 1;              ///< [21]    Enables SF attribute swizzling
        u32 number_of_sf_output_attributes : 6;         ///< [27:22] Count of attributes passed from SF to WM [0,32]
        u32 force_vertex_urb_entry_read_offset : 1;    ///< [28]    Workaround override
        u32 force_vertex_urb_entry_read_length : 1;    ///< [29]    Workaround override
        u32 reserved1_30 : 2;                          ///< [31:30] MBZ

        // DWord 2
        u32 point_sprite_texture_coordinate_enable : 32; ///< [31:0] Bitmask for attributes 0..31

        // DWord 3
        u32 constant_interpolation_enable : 32;          ///< [31:0] Bitmask for constant interpolation

        // DWord 4 (Active Component Formats for Attributes 0..15)
        u32 attribute_0_active_component_format : 2;     ///< [1:0]
        u32 attribute_1_active_component_format : 2;     ///< [3:2]
        u32 attribute_2_active_component_format : 2;     ///< [5:4]
        u32 attribute_3_active_component_format : 2;     ///< [7:6]
        u32 attribute_4_active_component_format : 2;     ///< [9:8]
        u32 attribute_5_active_component_format : 2;     ///< [11:10]
        u32 attribute_6_active_component_format : 2;     ///< [13:12]
        u32 attribute_7_active_component_format : 2;     ///< [15:14]
        u32 attribute_8_active_component_format : 2;     ///< [17:16]
        u32 attribute_9_active_component_format : 2;     ///< [19:18]
        u32 attribute_10_active_component_format : 2;    ///< [21:20]
        u32 attribute_11_active_component_format : 2;    ///< [23:22]
        u32 attribute_12_active_component_format : 2;    ///< [25:24]
        u32 attribute_13_active_component_format : 2;    ///< [27:26]
        u32 attribute_14_active_component_format : 2;    ///< [29:28]
        u32 attribute_15_active_component_format : 2;    ///< [31:30]

        // DWord 5 (Active Component Formats for Attributes 16..31)
        u32 attribute_16_active_component_format : 2;    ///< [1:0]
        u32 attribute_17_active_component_format : 2;    ///< [3:2]
        u32 attribute_18_active_component_format : 2;    ///< [5:4]
        u32 attribute_19_active_component_format : 2;    ///< [7:6]
        u32 attribute_20_active_component_format : 2;    ///< [9:8]
        u32 attribute_21_active_component_format : 2;    ///< [11:10]
        u32 attribute_22_active_component_format : 2;    ///< [13:12]
        u32 attribute_23_active_component_format : 2;    ///< [15:14]
        u32 attribute_24_active_component_format : 2;    ///< [17:16]
        u32 attribute_25_active_component_format : 2;    ///< [19:18]
        u32 attribute_26_active_component_format : 2;    ///< [21:20]
        u32 attribute_27_active_component_format : 2;    ///< [23:22]
        u32 attribute_28_active_component_format : 2;    ///< [25:24]
        u32 attribute_29_active_component_format : 2;    ///< [27:26]
        u32 attribute_30_active_component_format : 2;    ///< [29:28]
        u32 attribute_31_active_component_format : 2;    ///< [31:30]
    } __attribute__((packed));

    u32 raw[6];

    /**
     * @brief Creates a default-initialized 3DSTATE_SBE command.
     */
    [[nodiscard]] static constexpr STATE_SBE create() {
        STATE_SBE cmd{};
        cmd.dword_length = 0x4; // 6 DWords total - 2 = 4
        cmd.sub_opcode   = SUBOP_3DSTATE_SBE;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_SBE command configured for basic 3D rendering.
     *
     * Disables attribute swizzling (passthrough mode) and configures URB read length.
     *
     * @param num_attributes Number of SF output attributes (excluding position).
     * @param urb_read_length Amount of URB data read per vertex (in 256-bit units).
     */
    [[nodiscard]] static constexpr STATE_SBE create_default(u32 num_attributes = 1, u32 urb_read_length = 1) {
        STATE_SBE cmd = create();
        cmd.number_of_sf_output_attributes = num_attributes;
        cmd.vertex_urb_entry_read_length = urb_read_length;
        cmd.attribute_swizzle_enable = 0; // Passthrough mode
        cmd.point_sprite_texture_coordinate_origin = ORIGIN_UPPER_LEFT;
        return cmd;
    }
};

static_assert(sizeof(STATE_SBE) == 24, "STATE_SBE must be 6 DWords (24 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_SBE_H