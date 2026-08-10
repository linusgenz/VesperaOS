// cmd_3dstate_te.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_TE_H
#define VESPERAOS_CMD_3DSTATE_TE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_TE command (4 DWords).
 *
 * Controls the Tessellation Engine (TE) stage hardware in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 225-228 (3DSTATE_TE)
 */
union STATE_TE {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_TE = 0x1C };

    enum TEMode : u32 { MODE_HW_TESS = 0x0 };

    enum TEDomain : u32 { DOMAIN_QUAD = 0x0, DOMAIN_TRI = 0x1, DOMAIN_ISOLINE = 0x2 };

    enum OutputTopology : u32 {
        OUTPUT_TOPOLOGY_POINT   = 0x0,
        OUTPUT_TOPOLOGY_LINE    = 0x1,
        OUTPUT_TOPOLOGY_TRI_CW  = 0x2,
        OUTPUT_TOPOLOGY_TRI_CCW = 0x3
    };

    enum Partitioning : u32 {
        PARTITIONING_INTEGER         = 0x0,
        PARTITIONING_ODD_FRACTIONAL  = 0x1,
        PARTITIONING_EVEN_FRACTIONAL = 0x2
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x2 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x1C (3DSTATE_TE)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 te_enable : 1;       ///< [0]     1 = Enabled, 0 = Disabled (pass-through)
        u32 te_mode : 2;         ///< [2:1]   TEMode enum (0 = HW_TESS)
        u32 reserved1_3 : 1;     ///< [3]     MBZ
        u32 te_domain : 2;       ///< [5:4]   TEDomain enum
        u32 reserved1_6 : 2;     ///< [7:6]   MBZ
        u32 output_topology : 2; ///< [9:8]   OutputTopology enum
        u32 reserved1_10 : 2;    ///< [11:10] MBZ
        u32 partitioning : 2;    ///< [13:12] Partitioning enum
        u32 reserved1_14 : 18;   ///< [31:14] MBZ

        // DWord 2
        f32 max_tessellation_factor_odd; ///< [31:0] IEEE_Float; Default: 63.0f (0x427C0000)

        // DWord 3
        f32 max_tessellation_factor_not_odd; ///< [31:0] IEEE_Float; Default: 64.0f (0x42800000)
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a default-initialized 3DSTATE_TE command.
     */
    [[nodiscard]] static constexpr STATE_TE create() {
        STATE_TE cmd{};
        cmd.dword_length                   = 0x2; // 4 DWords total - 2 = 2
        cmd.sub_opcode                     = SUBOP_3DSTATE_TE;
        cmd.opcode                         = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type                       = GFXPIPE_3D;
        cmd.command_type                   = CMD_GFXPIPE;
        cmd.max_tessellation_factor_odd     = 63.0f;
        cmd.max_tessellation_factor_not_odd = 64.0f;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_TE command that disables the Tessellation Engine stage (pass-through).
     */
    [[nodiscard]] static constexpr STATE_TE create_disabled() {
        STATE_TE cmd = create();
        cmd.te_enable = 0; // Disable TE stage
        return cmd;
    }
};

static_assert(sizeof(STATE_TE) == 16, "STATE_TE must be 4 DWords (16 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_TE_H