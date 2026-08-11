// cmd_3dstate_vf_statistics.h
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

#ifndef VESPERAOS_CMD_3DSTATE_VF_STATISTICS_H
#define VESPERAOS_CMD_3DSTATE_VF_STATISTICS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_VF_STATISTICS command (1 DWord).
 *
 * Controls pipeline statistics gathering in the Vertex Fetcher (VF) stage.
 * When enabled, increments IA_VERTICES_COUNT and IA_PRIMITIVES_COUNT.
 *
 * @see Intel PRM (3DSTATE_VF_STATISTICS)
 */
union STATE_VF_STATISTICS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_VF_STATISTICS = 0x0B };

    struct {
        // DWord 0
        u32 statistics_enable : 1; ///< [0]     1 = Enable IA_VERTICES_COUNT / IA_PRIMITIVES_COUNT
        u32 reserved0_1 : 15;      ///< [15:1]  MBZ
        u32 sub_opcode : 8;        ///< [23:16] Default: 0x0B (3DSTATE_VF_STATISTICS)
        u32 opcode : 3;            ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;          ///< [28:27] Default: 0x1 (GFXPIPE_SINGLE_DW)
        u32 command_type : 3;      ///< [31:29] Default: 0x3 (GFXPIPE)
    } __attribute__((packed));

    u32 raw[1];

    /**
     * @brief Creates a default-initialized 3DSTATE_VF_STATISTICS command.
     */
    [[nodiscard]] static constexpr STATE_VF_STATISTICS create() {
        STATE_VF_STATISTICS cmd{};
        cmd.sub_opcode   = SUBOP_3DSTATE_VF_STATISTICS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_SINGLE_DW;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_VF_STATISTICS command with statistics enabled.
     */
    [[nodiscard]] static constexpr STATE_VF_STATISTICS create_enabled() {
        STATE_VF_STATISTICS cmd = create();
        cmd.statistics_enable   = 1;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_VF_STATISTICS command with statistics disabled.
     */
    [[nodiscard]] static constexpr STATE_VF_STATISTICS create_disabled() {
        STATE_VF_STATISTICS cmd = create();
        cmd.statistics_enable   = 0;
        return cmd;
    }
};

static_assert(sizeof(STATE_VF_STATISTICS) == 4, "STATE_VF_STATISTICS must be 1 DWord (4 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_VF_STATISTICS_H