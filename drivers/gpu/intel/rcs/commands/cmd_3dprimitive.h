// cmd_3dprimitive.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.08.26.
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

#ifndef VESPERAOS_CMD_3DPRIMITIVE_H
#define VESPERAOS_CMD_3DPRIMITIVE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DPRIMITIVE command (7 DWords).
 *
 * Used to submit 3D primitives to be processed by the 3D pipeline. The parameters
 * passed in this command are forwarded to the Vertex Fetch function, which will
 * generate vertex data structures and store them in the URB.
 *
 * @see 3DPRIMITIVE spec
 */
union CMD_3DPRIMITIVE {
    enum CommandSubOpcode : u32 { SUBOP_3DPRIMITIVE = 0x0 };

    enum VertexAccessType : u32 {
        ACCESS_SEQUENTIAL = 0x0,  ///< VERTEXDATA buffers are accessed sequentially
        ACCESS_RANDOM = 0x1       ///< VERTEXDATA buffers are accessed randomly via an Index Buffer
    };

    struct {
        // DWord 0
        u32 dword_length : 8;              ///< [7:0]   Default: 0x5 (7 DWords - 2)
        u32 predicate_enable : 1;          ///< [8]     If set, execution depends on MI Predicate state
        u32 uav_coherency_required : 1;    ///< [9]     U1; may cause a flush due to UAV coherency
        u32 indirect_parameter_enable : 1; ///< [10]    If set, DW 2-5 are replaced by 3DPRIM_xxx MMIO registers
        u32 reserved0_11 : 5;              ///< [15:11] MBZ
        u32 sub_opcode : 8;                ///< [23:16] Default: 0x0 (3DPRIMITIVE)
        u32 opcode : 3;                    ///< [26:24] Default: 0x3 (3DPRIMITIVE)
        u32 sub_type : 2;                  ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;              ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 primitive_topology_type : 6;   ///< [5:0]   Ignored (topology is specified via 3DSTATE_VF_TOPOLOGY)
        u32 reserved1_6 : 2;               ///< [7:6]   MBZ
        u32 vertex_access_type : 1;        ///< [8]     VertexAccessType enum (SEQUENTIAL or RANDOM)
        u32 end_offset_enable : 1;         ///< [9]     If set, Vertex Count Per Instance is ignored
        u32 reserved1_10 : 22;             ///< [31:10] MBZ

        // DWord 2
        u32 vertex_count_per_instance;     ///< [31:0]  U32; Count of vertices to generate per instance

        // DWord 3
        u32 start_vertex_location;         ///< [31:0]  U32; Structure index for SEQUENTIAL, index buffer index for RANDOM

        // DWord 4
        u32 instance_count;                ///< [31:0]  U32; Number of instances (0 = no-op)

        // DWord 5
        u32 start_instance_location;       ///< [31:0]  U32; Structure index into Vertex Buffers for Instancing

        // DWord 6
        i32 base_vertex_location;          ///< [31:0]  S31; Signed index structure bias (added to read index buffer values)
    } __attribute__((packed));

    u32 raw[7];

    /**
     * @brief Creates a default-initialized 3DPRIMITIVE command.
     */
    [[nodiscard]] static constexpr CMD_3DPRIMITIVE create() {
        CMD_3DPRIMITIVE cmd{};
        cmd.dword_length = 0x5; // 7 DWords total - 2 = 5
        cmd.sub_opcode   = SUBOP_3DPRIMITIVE;
        cmd.opcode       = OPCODE_3DPRIMITIVE;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }
};

static_assert(sizeof(CMD_3DPRIMITIVE) == 28, "CMD_3DPRIMITIVE must be 7 DWords (28 bytes)");

#endif  // VESPERAOS_CMD_3DPRIMITIVE_H