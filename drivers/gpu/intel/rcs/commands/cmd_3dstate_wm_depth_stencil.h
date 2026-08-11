// cmd_3dstate_wm_depth_stencil.h
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

#ifndef VESPERAOS_CMD_3DSTATE_WM_DEPTH_STENCIL_H
#define VESPERAOS_CMD_3DSTATE_WM_DEPTH_STENCIL_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_WM_DEPTH_STENCIL command (4 DWords).
 *
 * Configures the depth and stencil test parameters, operations, masks,
 * and reference values for pixel processing.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 267-270 (3DSTATE_WM_DEPTH_STENCIL)
 */
union STATE_WM_DEPTH_STENCIL {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_WM_DEPTH_STENCIL = 0x4E };

    enum CompareFunction : u32 {
        COMPARE_ALWAYS   = 0x0,
        COMPARE_NEVER    = 0x1,
        COMPARE_LESS     = 0x2,
        COMPARE_EQUAL    = 0x3,
        COMPARE_LEQUAL   = 0x4,
        COMPARE_GREATER  = 0x5,
        COMPARE_NOTEQUAL = 0x6,
        COMPARE_GEQUAL   = 0x7,
    };

    enum StencilOp : u32 {
        STENCILOP_KEEP    = 0x0,
        STENCILOP_ZERO    = 0x1,
        STENCILOP_REPLACE = 0x2,
        STENCILOP_INCRSAT = 0x3,
        STENCILOP_DECRSAT = 0x4,
        STENCILOP_INVERT  = 0x5,
        STENCILOP_INCR    = 0x6,
        STENCILOP_DECR    = 0x7,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x2 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x4E (3DSTATE_WM_DEPTH_STENCIL)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 depth_buffer_write_enable : 1;            ///< [0]     Depth buffer write enable
        u32 depth_test_enable : 1;                    ///< [1]     Depth test enable
        u32 stencil_buffer_write_enable : 1;          ///< [2]     Stencil buffer write enable
        u32 stencil_test_enable : 1;                  ///< [3]     Stencil test enable
        u32 double_sided_stencil_enable : 1;          ///< [4]     Double sided stencil enable
        u32 depth_test_function : 3;                  ///< [7:5]   CompareFunction enum
        u32 stencil_test_function : 3;                ///< [10:8]  CompareFunction enum
        u32 backface_stencil_pass_depth_pass_op : 3;  ///< [13:11] StencilOp enum
        u32 backface_stencil_pass_depth_fail_op : 3;  ///< [16:14] StencilOp enum
        u32 backface_stencil_fail_op : 3;             ///< [19:17] StencilOp enum
        u32 backface_stencil_test_function : 3;       ///< [22:20] CompareFunction enum
        u32 stencil_pass_depth_pass_op : 3;           ///< [25:23] StencilOp enum
        u32 stencil_pass_depth_fail_op : 3;           ///< [28:26] StencilOp enum
        u32 stencil_fail_op : 3;                      ///< [31:29] StencilOp enum

        // DWord 2
        u32 backface_stencil_write_mask : 8;          ///< [7:0]   Backface stencil write mask
        u32 backface_stencil_test_mask : 8;           ///< [15:8]  Backface stencil test mask
        u32 stencil_write_mask : 8;                   ///< [23:16] Stencil write mask
        u32 stencil_test_mask : 8;                    ///< [31:24] Stencil test mask

        // DWord 3
        u32 backface_stencil_reference_value : 8;     ///< [7:0]   Backface stencil reference value
        u32 stencil_reference_value : 8;              ///< [15:8]  Stencil reference value
        u32 reserved3_16 : 16;                        ///< [31:16] MBZ
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a default-initialized 3DSTATE_WM_DEPTH_STENCIL command.
     */
    [[nodiscard]] static constexpr STATE_WM_DEPTH_STENCIL create() {
        STATE_WM_DEPTH_STENCIL cmd{};
        cmd.dword_length = 0x2; // 4 DWords total - 2 = 2
        cmd.sub_opcode   = SUBOP_3DSTATE_WM_DEPTH_STENCIL;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_WM_DEPTH_STENCIL command configured for standard depth buffering.
     *
     * @param depth_test Enable depth test (default: true).
     * @param depth_write Enable depth writes (default: true).
     * @param depth_func Depth comparison function (default: COMPARE_LESS).
     */
    [[nodiscard]] static constexpr STATE_WM_DEPTH_STENCIL create_default(
        bool depth_test = true,
        bool depth_write = true,
        CompareFunction depth_func = COMPARE_LESS) {
        STATE_WM_DEPTH_STENCIL cmd = create();
        cmd.depth_test_enable = depth_test ? 1 : 0;
        cmd.depth_buffer_write_enable = depth_write ? 1 : 0;
        cmd.depth_test_function = depth_func;
        return cmd;
    }
};

static_assert(sizeof(STATE_WM_DEPTH_STENCIL) == 16, "STATE_WM_DEPTH_STENCIL must be 4 DWords (16 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_WM_DEPTH_STENCIL_H