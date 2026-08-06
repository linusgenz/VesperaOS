// gfx_pipeline_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.08.26.
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

#ifndef VESPERAOS_GFX_PIPELINE_REGS_H
#define VESPERAOS_GFX_PIPELINE_REGS_H

#include <vespera/types.h>

/**
 * @brief Pipeline Select Instruction (PIPELINE_SELECT).
 *
 * Used to specify which GPE pipeline is to be considered the 'current' active
 * pipeline (3D, Media, or GPGPU).
 *
 * @note Software must ensure all write caches are flushed through a stalling
 *       PIPE_CONTROL command followed by another PIPE_CONTROL command to invalidate
 *       read-only caches prior to programming PIPELINE_SELECT to change the mode.
 *
 * @note Issuing 3D-pipeline-specific commands when Media or GPGPU is selected
 *       (or vice versa) is UNDEFINED. Programming common non-pipeline commands
 *       (e.g., STATE_BASE_ADDRESS) is allowed in all modes.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 1112–1114 (PIPELINE_SELECT)
 */
union PIPELINE_SELECT {
    enum PipelineSelection : u32 {
        PIPELINE_3D = 0b00,      ///< 3D pipeline selected
        PIPELINE_MEDIA = 0b01,   ///< Media pipeline selected
        PIPELINE_GPGPU = 0b10,   ///< GPGPU pipeline selected
    };

    enum CommandType : u32 {
        CMD_GFXPIPE = 0x3,
    };

    enum CommandSubType : u32 {
        GFXPIPE_SINGLE_DW = 0x1,
    };

    enum CommandOpcode : u32 {
        GFXPIPE_NONPIPELINED = 0x1,
    };

    enum CommandSubOpcode : u32 {
        SUBOP_PIPELINE_SELECT = 0x04,
    };

    struct {
        PipelineSelection pipeline_selection : 2;  ///< [1:0]   Pipeline Selection (see PipelineSelection enum)
        u32 render_slice_common_power_gate : 1;    ///< [2]     Render Slice common Power Gate Enable (Mask bit [10])
        u32 render_sampler_power_gate : 1;         ///< [3]     Render Sampler Power Gate Enable (Mask bit [11])
        u32 media_sampler_dop_clock_gate : 1;      ///< [4]     Media Sampler DOP Clock Gate Enable (Mask bit [12])
        u32 force_media_awake : 1;                 ///< [5]     Force Media Awake (Mask bit [13])
        u32 reserved6_7 : 2;                       ///< [7:6]   Reserved (MBZ)
        u32 mask_bits : 8;                         ///< [15:8]  Mask Bits (Must be set to modify corresponding bits [7:0])
        u32 sub_opcode : 8;                        ///< [23:16] 3D Command Sub Opcode (Default: 0x04)
        u32 opcode : 3;                            ///< [26:24] 3D Command Opcode (Default: 0x01 = GFXPIPE_NONPIPELINED)
        u32 sub_type : 2;                          ///< [28:27] Command SubType (Default: 0x01 = GFXPIPE_SINGLE_DW)
        u32 command_type : 3;                      ///< [31:29] Command Type (Default: 0x03 = GFXPIPE)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Mask bit offsets within bits [15:8] required by hardware to update bits [7:0].
     */
    static constexpr u8 MASK_PIPELINE_SELECT = (1 << 0) | (1 << 1);  ///< Mask for bits [1:0] (Bits [9:8])
    static constexpr u8 MASK_RENDER_SLICE_PG = (1 << 2);             ///< Mask for bit [2]   (Bit [10])
    static constexpr u8 MASK_RENDER_SAMPLER_PG = (1 << 3);           ///< Mask for bit [3]   (Bit [11])
    static constexpr u8 MASK_MEDIA_SAMPLER_CG = (1 << 4);            ///< Mask for bit [4]   (Bit [12])
    static constexpr u8 MASK_FORCE_MEDIA_AWAKE = (1 << 5);           ///< Mask for bit [5]   (Bit [13])

    /**
     * @brief Constructs a pre-initialized PIPELINE_SELECT command for a target pipeline.
     * @param select Target pipeline mode (3D, Media, or GPGPU).
     */
    [[nodiscard]] static constexpr PIPELINE_SELECT create(PipelineSelection select) {
        PIPELINE_SELECT cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_SINGLE_DW;
        cmd.opcode = GFXPIPE_NONPIPELINED;
        cmd.sub_opcode = SUBOP_PIPELINE_SELECT;
        cmd.pipeline_selection = select;
        cmd.mask_bits = MASK_PIPELINE_SELECT;
        return cmd;
    }
};

static_assert(sizeof(PIPELINE_SELECT) == 4, "PIPELINE_SELECT command must be 32 bits");

#endif  // VESPERAOS_GFX_PIPELINE_REGS_H