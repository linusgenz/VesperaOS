// cmd_3dstate_multisample.h
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

#ifndef VESPERAOS_CMD_3DSTATE_MULTISAMPLE_H
#define VESPERAOS_CMD_3DSTATE_MULTISAMPLE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_MULTISAMPLE command (2 DWords).
 *
 * Specifies multisample state associated with the current render target / depth buffer.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 141-142 (3DSTATE_MULTISAMPLE)
 */
union STATE_MULTISAMPLE {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_MULTISAMPLE = 0x0D };

    enum NumberOfMultisamples : u32 {
        NUMSAMPLES_1  = 0x0,
        NUMSAMPLES_2  = 0x1,
        NUMSAMPLES_4  = 0x2,
        NUMSAMPLES_8  = 0x3,
        NUMSAMPLES_16 = 0x4,
    };

    enum PixelLocation : u32 {
        CENTER    = 0x0,
        UL_CORNER = 0x1,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x0D (3DSTATE_MULTISAMPLE)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 reserved1_0 : 1;                     ///< [0]     MBZ
        u32 number_of_multisamples : 3;          ///< [3:1]   NumberOfMultisamples enum
        u32 pixel_location : 1;                  ///< [4]     PixelLocation enum (0=CENTER, 1=UL_CORNER)
        u32 pixel_position_offset_enable : 1;    ///< [5]     Enables 0.5 offset in X & Y
        u32 reserved1_6 : 26;                    ///< [31:6]  MBZ
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_MULTISAMPLE command.
     */
    [[nodiscard]] static constexpr STATE_MULTISAMPLE create() {
        STATE_MULTISAMPLE cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = SUBOP_3DSTATE_MULTISAMPLE;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_MULTISAMPLE command configured for single-sample (1x) rendering.
     *
     * @param samples Number of multisamples (default: NUMSAMPLES_1).
     * @param location Pixel evaluation location (default: CENTER).
     */
    [[nodiscard]] static constexpr STATE_MULTISAMPLE create_default(
        NumberOfMultisamples samples = NUMSAMPLES_1,
        PixelLocation location = CENTER) {
        STATE_MULTISAMPLE cmd = create();
        cmd.number_of_multisamples = samples;
        cmd.pixel_location = location;
        cmd.pixel_position_offset_enable = 0;
        return cmd;
    }
};

static_assert(sizeof(STATE_MULTISAMPLE) == 8, "STATE_MULTISAMPLE must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_MULTISAMPLE_H