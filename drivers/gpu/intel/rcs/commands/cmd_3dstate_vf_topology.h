// cmd_3dstate_vf_topology.h
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

#ifndef VESPERAOS_CMD_3DSTATE_VF_TOPOLOGY_H
#define VESPERAOS_CMD_3DSTATE_VF_TOPOLOGY_H

#include <vespera/types.h>

#include "cmd_common.h"
#include "../state/primitive_topology.h"

/**
 * @brief 3DSTATE_VF_TOPOLOGY command (2 DWords).
 *
 * This command specifies the VF stage's Topology state which can be used to override the Primitive Topology
 * Type in subsequent 3DPRIMITIVE commands.
 */
union STATE_VF_TOPOLOGY {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_VF_TOPOLOGY = 0x4B };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x4B (3DSTATE_VF_TOPOLOGY)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 primitive_topology_type : 6; ///< [5:0] This field specifies the VF stage's Topology state.
                                         ///< Use the PrimTopoType enum
        u32 reserved1_6 : 26;            ///< [31:6]  MBZ
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_VF_TOPOLOGY command.
     */
    [[nodiscard]] static constexpr STATE_VF_TOPOLOGY create(PrimTopoType topo) {
        STATE_VF_TOPOLOGY cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = SUBOP_3DSTATE_VF_TOPOLOGY;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        cmd.primitive_topology_type = topo;
        return cmd;
    }
};

static_assert(sizeof(STATE_VF_TOPOLOGY) == 8, "STATE_VF_TOPOLOGY must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_VF_TOPOLOGY_H