// cmd_3dstate_depth_buffer.h
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

#ifndef VESPERAOS_CMD_DEPTH_BUFFER_H
#define VESPERAOS_CMD_DEPTH_BUFFER_H

#include <vespera/types.h>

/**
 * @brief 3DSTATE_DEPTH_BUFFER command (8 DWords).
 *
 * Lives directly in the command stream (unlike SF_CLIP_VIEWPORT,
 * CC_VIEWPORT, or SCISSOR_RECT, which are Dynamic State Base
 * Address-relative structures only pointed at from the stream).
 *
 * @note Surface Type SURFTYPE_1D is not allowed for the depth/stencil
 *       surface. Workaround for a 1D render target: set the depth
 *       buffer's Surface Type to SURFTYPE_2D with Height = 1 (depth uses
 *       legacy TileY, stencil uses TileW in that case).
 * @note Hierarchical Depth Buffer Enable requires Software Tiled
 *       Rendering Mode == NORMAL, and must be disabled if Early Depth
 *       Test is disabled, if Surface Type is SURFTYPE_NULL, or if
 *       Surface Type is SURFTYPE_1D.
 * @note Depth Write Enable / Stencil Write Enable here must be paired
 *       with the matching enables in DEPTH_STENCIL_STATE for writes to
 *       actually occur.
 * @note The minimum Surface Pitch must be a multiple of the tile pitch,
 *       in the range [128B, 128KB]; see Programming Notes in the PRM for
 *       the Cu/Cv/W0-based minimum-pitch formula.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 54-60 (3DSTATE_DEPTH_BUFFER)
 */
union DEPTH_BUFFER {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_DEPTH_BUFFER = 0x05 };

    enum SurfaceType : u32 {
        SURFTYPE_2D = 0x1,
        SURFTYPE_CUBE = 0x3,
        SURFTYPE_NULL = 0x7,
    };

    enum DepthFormat : u32 {
        DEPTH_D32_FLOAT = 0x1,
        DEPTH_D24_UNORM_X8_UINT = 0x3,
        DEPTH_D16_UNORM = 0x5,
    };

    enum TiledResourceMode : u32 {
        TRMODE_NONE = 0x0,
        TRMODE_TILEYF = 0x1,
        TRMODE_TILEYS = 0x2,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;   ///< [7:0]   Default: 0x6 (Length = DWords - 2 = 8 - 2)
        u32 reserved0_8 : 8;    ///< [15:8]  MBZ
        u32 sub_opcode : 8;     ///< [23:16] Default: 0x05
        u32 opcode : 3;         ///< [26:24] Default: 0x0
        u32 sub_type : 2;       ///< [28:27] Default: 0x3
        u32 command_type : 3;   ///< [31:29] Default: 0x3

        // DWord 1
        u32 surface_pitch : 18;      ///< [17:0]  U18-1, Pitch in (Bytes-1)
        u32 surface_format : 3;      ///< [20:18] DepthFormat enum
        u32 reserved1_21 : 1;        ///< [21]    MBZ
        u32 hiz_enable : 1;          ///< [22]    Hierarchical Depth Buffer Enable
        u32 reserved1_23 : 1;        ///< [23]    MBZ
        u32 reserved1_24 : 3;        ///< [26:24] MBZ
        u32 stencil_write_enable : 1; ///< [27]    Stencil Write Enable
        u32 depth_write_enable : 1;  ///< [28]    Depth Write Enable
        u32 surface_type : 3;        ///< [31:29] SurfaceType enum

        // DWord 2..3
        u64 surface_base_address;  ///< [63:0] GraphicsAddress63-0
                                    ///< Programming Notes: uncached Main Memory only.
                                    ///< If tiled: must follow Per-Surface Tiling
                                    ///< Alignment Rules. If linear: 64-byte aligned.

        // DWord 4
        u32 lod : 4;             ///< [3:0]   U4, LOD units, legal range [0,14]
        u32 width : 14;          ///< [17:4]  U14-1, width of surface (base MIP level) - 1
        u32 height : 14;         ///< [31:18] U14-1, height of surface (base MIP level) - 1

        // DWord 5
        u32 depth_buffer_ocs : 7;   ///< [6:0]   MEMORY_OBJECT_CONTROL_STATE
        u32 reserved5_7 : 3;        ///< [9:7]   MBZ
        u32 min_array_element : 11; ///< [20:10] U11, minimum array element / min R coord
        u32 depth : 11;             ///< [31:21] U11-1, array elements / volume depth - 1

        // DWord 6
        u32 reserved6_0 : 26;          ///< [25:0]  MBZ
        u32 mip_tail_start_lod : 4;    ///< [29:26] U4, LOD units; ignored unless tiled
        u32 tiled_resource_mode : 2;   ///< [31:30] TiledResourceMode enum

        // DWord 7
        u32 surface_qpitch : 15;         ///< [14:0]  QPitch[16:2], multiples of 4
        u32 reserved7_15 : 6;            ///< [20:15] MBZ
        u32 render_target_view_extent : 11; ///< [31:21] U11-1
    } __attribute__((packed));

    u32 raw[8];

    /**
     * @brief Creates a simple, non-MIP-mapped, non-arrayed, non-tiled
     *        2D depth buffer covering the full render target.
     *
     * @param base_address     Graphics address of the depth surface.
     * @param width, height    Surface size in pixels.
     * @param pitch_bytes      Surface pitch in bytes; must be a multiple
     *                         of the tile pitch when tiled, [128B,128KB].
     * @param format           One of DepthFormat.
     */
    [[nodiscard]] static DEPTH_BUFFER create_simple_2d(
        u64 base_address, u32 width, u32 height, u32 pitch_bytes,
        DepthFormat format = DEPTH_D32_FLOAT
    ) {
        DEPTH_BUFFER cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_3DSTATE_DEPTH_BUFFER;
        cmd.dword_length = 0x6;

        cmd.surface_type = SURFTYPE_2D;
        cmd.surface_format = format;
        cmd.hiz_enable = 0;
        cmd.stencil_write_enable = 0;
        cmd.depth_write_enable = 1;
        cmd.surface_pitch = pitch_bytes - 1;

        cmd.surface_base_address = base_address;

        cmd.width = width - 1;
        cmd.height = height - 1;
        cmd.lod = 0;

        cmd.depth = 0;              // non-array
        cmd.min_array_element = 0;
        cmd.depth_buffer_ocs = 0;

        cmd.tiled_resource_mode = TRMODE_NONE;
        cmd.mip_tail_start_lod = 0;

        cmd.render_target_view_extent = 0;
        cmd.surface_qpitch = 0;

        return cmd;
    }
};

static_assert(sizeof(DEPTH_BUFFER) == 32, "DEPTH_BUFFER must be 8 DWords (32 bytes)");

#endif  // VESPERAOS_CMD_DEPTH_BUFFER_H