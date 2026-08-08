// cmd_render_surface_state.h
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

#ifndef VESPERAOS_CMD_RENDER_SURFACE_STATE_H
#define VESPERAOS_CMD_RENDER_SURFACE_STATE_H

#include <vespera/types.h>

/**
 * @brief RENDER_SURFACE_STATE structure (16 DWords / 512 bits).
 *
 * Describes a surface (render target, texture, buffer, or media surface)
 * for the sampling engine and the Color Calculator / render cache. Lives in
 * memory (Surface State Base Address-relative), NOT in the command
 * stream — 3DSTATE_BINDING_TABLE_POINTERS_* only points at an array of
 * BINDING_TABLE_STATE entries, each of which in turn points at one of
 * these structures.
 *
 * @note Must be 64-byte aligned in memory (BINDING_TABLE_STATE's Surface
 *       State Pointer is a 64-byte-granular offset).
 * @note DWord 6 has an alternate interpretation for planar YUV surfaces
 *       (Y/X Offset for U or UV Plane) that overlaps Auxiliary Surface
 *       Mode/Pitch; DWord 10 has an alternate interpretation (Quilt
 *       Width/Height, Auxiliary Table Index) that overlaps the low bits of
 *       Auxiliary Surface Base Address. Only one interpretation applies at
 *       a time depending on surface format and Auxiliary Surface Mode.
 * @note DWord 12-15 are reused as Red/Green/Blue/Alpha Clear Color when
 *       Auxiliary Surface Mode enables fast-clear tracking (AUX_CCS_D,
 *       AUX_CCS_E), or as Hierarchical Depth Clear Value (DWord 12 only)
 *       for AUX_HIZ surfaces.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 467-498 (RENDER_SURFACE_STATE)
 */
struct RENDER_SURFACE_STATE {
    enum SurfaceType : u32 {
        SURFTYPE_1D     = 0x0,
        SURFTYPE_2D     = 0x1,
        SURFTYPE_3D     = 0x2,
        SURFTYPE_CUBE   = 0x3,
        SURFTYPE_BUFFER = 0x4,
        SURFTYPE_STRBUF = 0x5,
        SURFTYPE_NULL   = 0x7,
    };

    enum TileMode : u32 {
        TILEMODE_LINEAR = 0x0,
        TILEMODE_WMAJOR = 0x1,
        TILEMODE_XMAJOR = 0x2,
        TILEMODE_YMAJOR = 0x3,
    };

    enum SurfaceAlignment : u32 {
        ALIGN_4  = 0x1,
        ALIGN_8  = 0x2,
        ALIGN_16 = 0x3,
    };

    enum RenderCacheReadWriteMode : u32 {
        CACHE_WRITE_ONLY = 0x0,
        CACHE_READ_WRITE = 0x1,
    };

    enum MultisampleCount : u32 {
        MULTISAMPLECOUNT_1  = 0x0,
        MULTISAMPLECOUNT_2  = 0x1,
        MULTISAMPLECOUNT_4  = 0x2,
        MULTISAMPLECOUNT_8  = 0x3,
        MULTISAMPLECOUNT_16 = 0x4,
    };

    enum AuxiliarySurfaceMode : u32 {
        AUX_NONE   = 0x0,
        AUX_CCS_D  = 0x1,
        AUX_APPEND = 0x2,
        AUX_HIZ    = 0x3,
        AUX_CCS_E  = 0x5,
    };

    enum TiledResourceMode : u32 {
        TRMODE_NONE   = 0x0,
        TRMODE_TILEYF = 0x1,
        TRMODE_TILEYS = 0x2,
    };

    enum ShaderChannelSelect : u32 {
        SCS_ZERO  = 0x0,
        SCS_ONE   = 0x1,
        SCS_RED   = 0x4,
        SCS_GREEN = 0x5,
        SCS_BLUE  = 0x6,
        SCS_ALPHA = 0x7,
    };

    // DWord 0
    u32 cube_face_enable_positive_z : 1; ///< [0]     Cube map face enable
    u32 cube_face_enable_negative_z : 1; ///< [1]
    u32 cube_face_enable_positive_y : 1; ///< [2]
    u32 cube_face_enable_negative_y : 1; ///< [3]
    u32 cube_face_enable_positive_x : 1; ///< [4]
    u32 cube_face_enable_negative_x : 1; ///< [5]
    u32 media_boundary_pixel_mode   : 2;
    ///< [7:6]   0 = Normal, 2 = Progressive Frame,
                                              ///< 3 = Interlaced Frame
    u32 render_cache_read_write_mode         : 1; ///< [8]     RenderCacheReadWriteMode enum
    u32 sampler_l2_out_of_order_mode_disable : 1;
    ///< [9] Disable; if disabled, formats that
                                              ///< would bypass L2 are cached in L2 and sent
                                              ///< in order to L1
    u32 vertical_line_stride_offset  : 1; ///< [10]
    u32 vertical_line_stride         : 1; ///< [11]
    u32 tile_mode                    : 2; ///< [13:12] TileMode enum
    u32 surface_horizontal_alignment : 2; ///< [15:14] SurfaceAlignment enum
    u32 surface_vertical_alignment   : 2; ///< [17:16] SurfaceAlignment enum
    u32 surface_format               : 9; ///< [26:18] SURFACE_FORMAT enum (external)
    u32 astc_enable                  : 1;
    ///< [27]    Boolean; selects ASTC vs. legacy
                                              ///< Surface Format encoding
    u32 surface_array : 1; ///< [28]    Boolean
    u32 surface_type  : 3; ///< [31:29] SurfaceType enum

    // DWord 1
    u32 surface_qpitch : 15;
    ///< [14:0]  U15, QPitch[16:2], row pitch between array
                                   ///< slices/depth planes, multiples of 4
    u32 reserved1_15   : 4; ///< [18:15] MBZ
    u32 base_mip_level : 5; ///< [23:19] U4.1, base MIP level to use (fixed point)
    u32 mocs           : 7; ///< [30:24] MEMORY_OBJECT_CONTROL_STATE
    u32 reserved1_31   : 1; ///< [31]    MBZ

    // DWord 2
    u32 width        : 14; ///< [13:0]  U14-1, width of surface (base MIP level) - 1
    u32 reserved2_14 : 2;  ///< [15:14] MBZ
    u32 height       : 14; ///< [29:16] U14-1, height of surface (base MIP level) - 1
    u32 reserved2_30 : 2;  ///< [31:30] MBZ

    // DWord 3
    u32 surface_pitch : 18; ///< [17:0]  U18-1, pitch in (bytes - 1)
    u32 reserved3_18  : 3;  ///< [20:18] MBZ
    u32 depth         : 11; ///< [31:21] U11-1, array elements / volume depth - 1

    // DWord 4
    u32 multisample_position_palette_index      : 3;  ///< [2:0]  Index into MULTISAMPLE_POSITION_PALETTE
    u32 number_of_multisamples                  : 3;  ///< [5:3]  MultisampleCount enum
    u32 multisampled_surface_storage_format     : 1;  ///< [6]   0 = MSS, 1 = Depth/Stencil
    u32 render_target_view_extent               : 11; ///< [17:7] U11-1
    u32 minimum_array_element                   : 11; ///< [28:18] U11, min array element / min R coord
    u32 render_target_and_sample_unorm_rotation : 2;  ///< [30:29] 0/90/180/270 deg
    u32 reserved4_31                            : 1;  ///< [31]   MBZ

    // DWord 5
    u32 mip_count_lod        : 4; ///< [3:0]   U4, # of MIP levels - 1 / LOD for BUFFER surfaces
    u32 surface_min_lod      : 4; ///< [7:4]   U4, minimum LOD clamp
    u32 mip_tail_start_lod   : 4; ///< [11:8]  U4, ignored unless tiled resource
    u32 reserved5_12         : 2; ///< [13:12] MBZ
    u32 coherency_type       : 1; ///< [14]    0 = GPU coherent, 1 = IA coherent
    u32 reserved5_15         : 3; ///< [17:15] MBZ
    u32 tiled_resource_mode  : 2; ///< [19:18] TiledResourceMode enum
    u32 ewa_disable_for_cube : 1; ///< [20]    Boolean
    u32 y_offset             : 3; ///< [23:21] U3, vertical offset in units of 4 rows
    u32 reserved5_24         : 1; ///< [24]    MBZ
    u32 x_offset             : 7; ///< [31:25] U7, horizontal offset in units of 4 texels

    // DWord 6
    // NOTE: this DWord has two mutually-exclusive interpretations. The
    // Auxiliary Surface Mode / Pitch layout applies for non-planar
    // surfaces; the Y/X Offset for U or UV Plane layout applies for
    // planar YUV surfaces. Only one is valid at a time.
    union {
        struct {
            u32 auxiliary_surface_mode  : 3; ///< [2:0]   AuxiliarySurfaceMode enum
            u32 auxiliary_surface_pitch : 9;
            ///< [11:3]  U9-1, pitch of aux surface in
                                                           ///< 128-byte units (interpretation varies
                                                           ///< with Auxiliary Surface Mode)
            u32 reserved6_12             : 4;  ///< [15:12] MBZ
            u32 auxiliary_surface_qpitch : 15; ///< [30:16] U15, QPitch[16:2] for aux surface
            u32 separate_uv_plane_enable : 1;  ///< [31]    Boolean
        } aux;

        struct {
            u32 y_offset_for_u_or_uv_plane : 14; ///< [13:0]  U14, vertical offset (planar YUV)
            u32 reserved6_14               : 2;  ///< [15:14] MBZ
            u32 x_offset_for_u_or_uv_plane : 14; ///< [29:16] U14, horizontal offset (planar YUV)
            u32 reserved6_30               : 1;  ///< [30]    MBZ
            u32 separate_uv_plane_enable   : 1;  ///< [31]    Boolean
        } planar;

        u32 raw;
    } dword6;

    // DWord 7
    u32 resource_min_lod            : 12; ///< [11:0]  U4.8, minimum resource LOD clamp
    u32 reserved7_12                : 4;  ///< [15:12] MBZ
    u32 shader_channel_select_alpha : 3;  ///< [18:16] ShaderChannelSelect enum
    u32 shader_channel_select_blue  : 3;  ///< [21:19] ShaderChannelSelect enum
    u32 shader_channel_select_green : 3;  ///< [24:22] ShaderChannelSelect enum
    u32 shader_channel_select_red   : 3;  ///< [27:25] ShaderChannelSelect enum
    u32 reserved7_28                : 2;  ///< [29:28] MBZ
    u32 memory_compression_enable   : 1;  ///< [30]    Boolean
    u32 memory_compression_mode     : 1;  ///< [31]    0 = Horizontal, 1 = Vertical

    // DWord 8..9
    u64 surface_base_address;
    ///< [63:0] GraphicsAddress63-0, relative to Surface
                                   ///< State Base Address. Alignment/tiling rules per
                                   ///< Per-Surface Tiling Alignment Rules.

    // DWord 10
    // NOTE: mutually-exclusive interpretations, as with DWord 6. Quilt
    // Width/Height/Auxiliary Table Index apply to specific surface
    // classes; otherwise these bits are the low 20 bits of Auxiliary
    // Surface Base Address (bits [31:12] of that 64-bit field span
    // DWord 10 and DWord 11).
    union {
        struct {
            u32 quilt_width : 5; ///< [4:0]   U5, quilt image width (multi-view surfaces)
            u32 quilt_height : 5; ///< [9:5]   U5, quilt image height
            u32 reserved10_10 : 11; ///< [20:10] MBZ
            u32 auxiliary_table_index_for_media_compressed_surface : 11; ///< [31:21]
        } quilt;

        u32 auxiliary_surface_base_address_low;
        ///< [31:12] low bits of the 64-bit
                                                          ///< Auxiliary Surface Base Address
                                                          ///< (see DWord 10..11 note above);
                                                          ///< [11:0] MBZ
        u32 raw;
    } dword10;

    // DWord 11
    u32 y_offset_for_v_plane : 14; ///< [13:0]  U14, vertical offset for V plane (planar YUV)
    u32 reserved11_14        : 2;  ///< [15:14] MBZ
    u32 x_offset_for_v_plane : 14; ///< [29:16] U14, horizontal offset for V plane (planar YUV)
    u32 reserved11_30        : 2;  ///< [31:30] MBZ

    // DWord 12
    // NOTE: reused as Hierarchical Depth Clear Value (float) for AUX_HIZ
    // surfaces, or as Red Clear Color for AUX_CCS_D / AUX_CCS_E surfaces.
    union {
        float hierarchical_depth_clear_value; ///< AUX_HIZ surfaces only
        i32 red_clear_color;                  ///< AUX_CCS_D / AUX_CCS_E surfaces only
        u32 raw;
    } dword12;

    // DWord 13
    i32 green_clear_color; ///< AUX_CCS_D / AUX_CCS_E surfaces only

    // DWord 14
    i32 blue_clear_color; ///< AUX_CCS_D / AUX_CCS_E surfaces only

    // DWord 15
    i32 alpha_clear_color; ///< AUX_CCS_D / AUX_CCS_E surfaces only

    /**
     * @brief Builds a simple, non-tiled, non-MSAA 2D render target surface.
     *
     * Covers the common case: a linear 2D color surface with no cube
     * faces, no auxiliary surface, no MIP mapping, and default shader
     * channel swizzle (R/G/B/A passthrough).
     *
     * @param base_address  Graphics address of the surface, Surface State
     *                      Base Address-relative, 64-byte aligned if linear.
     * @param width, height Surface size in pixels.
     * @param pitch_bytes   Surface pitch in bytes.
     * @param format        SURFACE_FORMAT enum value (project-defined).
     * @param mocs          MEMORY_OBJECT_CONTROL_STATE value for this surface.
     */
    [[nodiscard]] static RENDER_SURFACE_STATE create_simple_2d(
        u64 base_address, u32 width, u32 height, u32 pitch_bytes, u32 format, u32 mocs = 0
    ) {
        RENDER_SURFACE_STATE state{};

        state.surface_type = SURFTYPE_2D;
        state.surface_format = format;
        state.astc_enable = 0;
        state.surface_array = 0;
        state.tile_mode = TILEMODE_LINEAR;
        state.surface_horizontal_alignment = ALIGN_4;
        state.surface_vertical_alignment = ALIGN_4;
        state.render_cache_read_write_mode = CACHE_READ_WRITE;

        state.surface_qpitch = 0;
        state.base_mip_level = 0;
        state.mocs = mocs;

        state.width = width - 1;
        state.height = height - 1;

        state.surface_pitch = pitch_bytes - 1;
        state.depth = 0; // non-array

        state.multisample_position_palette_index = 0;
        state.number_of_multisamples = MULTISAMPLECOUNT_1;
        state.multisampled_surface_storage_format = 0;
        state.render_target_view_extent = 0;
        state.minimum_array_element = 0;
        state.render_target_and_sample_unorm_rotation = 0;

        state.mip_count_lod = 0;
        state.surface_min_lod = 0;
        state.mip_tail_start_lod = 0;
        state.coherency_type = 0;
        state.tiled_resource_mode = TRMODE_NONE;
        state.ewa_disable_for_cube = 0;
        state.y_offset = 0;
        state.x_offset = 0;

        state.dword6.raw = 0;
        state.dword6.aux.auxiliary_surface_mode = AUX_NONE;

        state.resource_min_lod = 0;
        state.shader_channel_select_alpha = SCS_ALPHA;
        state.shader_channel_select_blue = SCS_BLUE;
        state.shader_channel_select_green = SCS_GREEN;
        state.shader_channel_select_red = SCS_RED;
        state.memory_compression_enable = 0;
        state.memory_compression_mode = 0;

        state.surface_base_address = base_address;

        state.dword10.raw = 0;
        state.y_offset_for_v_plane = 0;
        state.x_offset_for_v_plane = 0;

        state.dword12.raw = 0;
        state.green_clear_color = 0;
        state.blue_clear_color = 0;
        state.alpha_clear_color = 0;

        return state;
    }
};

static_assert(sizeof(RENDER_SURFACE_STATE) == 64, "RENDER_SURFACE_STATE must be 16 DWords (64 bytes)");

#endif  // VESPERAOS_CMD_RENDER_SURFACE_STATE_H
