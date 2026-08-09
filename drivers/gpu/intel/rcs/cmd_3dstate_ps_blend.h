// cmd_3dstate_ps_blend.h
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

#ifndef VESPERAOS_CMD_3DSTATE_PS_BLEND_H
#define VESPERAOS_CMD_3DSTATE_PS_BLEND_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_PS_BLEND command (2 DWords).
 *
 * Configures Color Buffer Blending for RT[0] and — critically — the
 * "Has Writeable RT" flag consumed by the Windower stage's
 * WM_INT::ThreadDispatchEnable formula (see IHD-OS-ICLLP-Vol 9, "3DSTATE_WM"
 * programming note): ThreadDispatchEnable requires
 * 3DSTATE_PS_EXTRA::PixelShaderValid AND
 * (!PixelShaderDoesNotWriteRT && 3DSTATE_PS_BLEND::HasWriteableRT). Without
 * this command (reset default: has_writeable_rt = 0), the PS stage is never
 * dispatched — regardless of how correctly 3DSTATE_PS/3DSTATE_PS_EXTRA
 * are configured.
 *
 * Lives directly in the command stream.
 *
 * IMPORTANT — C++ bitfield declaration order determines actual bit
 * placement, NOT the doc comments. Fields below are declared LSB-first
 * within DWord 1 to match the PRM's bit numbering.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 145-146 (3DSTATE_PS_BLEND)
 */
union STATE_PS_BLEND {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_PS_BLEND = 0x4D };

    /**
     * @brief 3D_Color_Buffer_Blend_Factor encoding.
     *
     * Values are not enumerated in the PRM excerpt available here — only
     * the bit positions/widths of the fields that use this format are
     * confirmed. Left unimplemented; add named constants here once the
     * actual factor table (Vol 2b enumerations, likely alongside
     * SURFACE_FORMAT) is available, the same way SURFACE_FORMAT values
     * were added to surface_format.h.
     */

    struct {
        // ====================================================================
        // DWord 0
        // ====================================================================
        u32 dword_length : 8; ///< [7:0]   Default: 0x0, Total Length - 2
        u32 reserved0_8  : 8; ///< [15:8]  MBZ
        u32 sub_opcode   : 8; ///< [23:16] Default: 0x4D (3DSTATE_PS_BLEND)
        u32 opcode       : 3; ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type     : 2; ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3; ///< [31:29] Default: 0x3 (GFXPIPE)

        // ====================================================================
        // DWord 1
        // ====================================================================
        u32 reserved1_0                    : 7; ///< [6:0]   MBZ
        u32 independent_alpha_blend_enable : 1; ///< [7]     1 = alpha fields below control alpha blend
        u32 alpha_test_enable              : 1; ///< [8]     AlphaTestEnable for RT[0]
        u32 destination_blend_factor       : 5; ///< [13:9]  3D_Color_Buffer_Blend_Factor
        u32 source_blend_factor            : 5; ///< [18:14] 3D_Color_Buffer_Blend_Factor
        u32 destination_alpha_blend_factor : 5; ///< [23:19] 3D_Color_Buffer_Blend_Factor
        u32 source_alpha_blend_factor      : 5; ///< [28:24] 3D_Color_Buffer_Blend_Factor
        u32 color_buffer_blend_enable      : 1; ///< [29]    1 = RT[0] has color buffer blend enabled
        u32 has_writeable_rt               : 1;
        ///< [30]    1 = at least one non-null RT with a
                                                    ///<         channel write enabled — REQUIRED for
                                                    ///<         WM_INT::ThreadDispatchEnable, see class doc
        u32 alpha_to_coverage_enable : 1; ///< [31]    AlphaToCoverage on RT[0]
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_PS_BLEND command — all
     *        blending/testing disabled, has_writeable_rt left at 0 (caller
     *        must explicitly set it via set_has_writeable_rt() or the field
     *        directly; a silently-0 default here would reproduce the exact
     *        bug this command exists to fix).
     */
    [[nodiscard]] static constexpr STATE_PS_BLEND create() {
        STATE_PS_BLEND cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode = SUBOP_3DSTATE_PS_BLEND;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Convenience constructor for the common "simple opaque render
     *        target, no blending, no alpha test" case — exactly what a
     *        constant-color kernel with no blend state needs.
     */
    [[nodiscard]] static constexpr STATE_PS_BLEND create_simple_writeable_rt() {
        STATE_PS_BLEND cmd = create();
        cmd.has_writeable_rt = 1;
        cmd.color_buffer_blend_enable = 0;
        cmd.alpha_test_enable = 0;
        cmd.alpha_to_coverage_enable = 0;
        cmd.independent_alpha_blend_enable = 0;
        return cmd;
    }
};

static_assert(sizeof(STATE_PS_BLEND)== 8, "STATE_PS_BLEND must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_PS_BLEND_H
