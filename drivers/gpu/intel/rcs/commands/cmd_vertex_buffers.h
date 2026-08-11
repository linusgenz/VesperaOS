// cmd_vertex_buffers.h
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

#ifndef VESPERAOS_CMD_VERTEX_BUFFERS_H
#define VESPERAOS_CMD_VERTEX_BUFFERS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief VERTEX_BUFFER_STATE structure (4 DWords).
 *
 * Describes one vertex buffer's location, pitch, and size for the VF
 * (Vertex Fetch) function. Embedded 1-to-33 times inside a
 * 3DSTATE_VERTEX_BUFFERS command.
 *
 * @note Buffer Starting Address must be byte-aligned; VBs can only live in
 *       linear (not tiled) memory.
 * @note Null Vertex Buffer must be set when Buffer Size is 0.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 730-731 (VERTEX_BUFFER_STATE)
 */
union VERTEX_BUFFER_STATE {
    struct {
        u32 buffer_pitch : 12;              ///< [11:0]  Buffer Pitch, bytes (U12)
        u32 reserved0_12 : 1;               ///< [12]    MBZ
        u32 null_vertex_buffer : 1;         ///< [13]    Null Vertex Buffer — fetches return 0
        u32 address_modify_enable : 1;      ///< [14]    1 = update Buffer Starting Address below
        u32 reserved0_15 : 1;               ///< [15]    MBZ
        u32 mocs : 7;                       ///< [22:16] Memory Object Control State
        u32 reserved0_23 : 3;               ///< [25:23] MBZ
        u32 vertex_buffer_index : 6;        ///< [31:26] U6, which VB slot this describes (0-32)

        u64 buffer_starting_address;        ///< DWord 1-2: [63:0] byte-aligned GraphicsAddress

        u32 buffer_size;                    ///< DWord 3: [31:0] size in bytes (0 = no valid data)
    } __attribute__((packed));

    u32 raw[4];

    /**
     * @brief Creates a default-initialized VERTEX_BUFFER_STATE for the given
     *        VB slot, pointing at a linear GGTT-resident buffer.
     *
     * @param vb_index      Vertex buffer slot (0-32).
     * @param addr          Byte-aligned GraphicsAddress of the first vertex.
     * @param pitch_bytes   Stride between consecutive vertex structures.
     * @param size_bytes    Total buffer size in bytes.
     * @param mocs_value    Memory Object Control State.
     */
    [[nodiscard]] static constexpr VERTEX_BUFFER_STATE create(
        u32 vb_index, u64 addr, u32 pitch_bytes, u32 size_bytes, u8 mocs_value = 0
    ) {
        VERTEX_BUFFER_STATE vb{};
        vb.vertex_buffer_index = vb_index;
        vb.mocs = mocs_value;
        vb.address_modify_enable = 1;
        vb.null_vertex_buffer = (size_bytes == 0) ? 1 : 0;
        vb.buffer_pitch = pitch_bytes;
        vb.buffer_starting_address = addr;
        vb.buffer_size = size_bytes;
        return vb;
    }
};

static_assert(sizeof(VERTEX_BUFFER_STATE) <= 16, "VERTEX_BUFFER_STATE must fit in 4 DWords (16 bytes)");

/**
 * @brief 3DSTATE_VERTEX_BUFFERS command header (variable length).
 *
 * Carries 1 to 33 embedded VERTEX_BUFFER_STATE structures (4 DWords each).
 * This header type only encodes DWord 0 — the caller appends N
 * VERTEX_BUFFER_STATE structures immediately after via ring_write_cmd() for
 * the header followed by ring_write_cmd() for each VERTEX_BUFFER_STATE, or
 * by writing raw DWords directly.
 *
 * @note DWord Length = 4*b - 1, where b = number of VERTEX_BUFFER_STATE
 *       structures included (NOT bytes — see PRM formula).
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 239-241 (3DSTATE_VERTEX_BUFFERS)
 */
union VERTEX_BUFFERS_HEADER {
    enum CommandSubOpcode : u32 {
        SUBOP_3DSTATE_VERTEX_BUFFERS = 0x08,
    };

    struct {
        u32 dword_length : 8;   ///< [7:0]   = 4*b - 1 (b = number of VBs included)
        u32 reserved0_8 : 8;    ///< [15:8]  MBZ
        u32 sub_opcode : 8;     ///< [23:16] Default: 0x08 (3DSTATE_VERTEX_BUFFERS)
        u32 opcode : 3;         ///< [26:24] Default: 0x0  (3DSTATE_PIPELINED)
        u32 sub_type : 2;       ///< [28:27] Default: 0x3  (GFXPIPE_3D)
        u32 command_type : 3;   ///< [31:29] Default: 0x3  (GFXPIPE)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Creates the command header for `num_buffers` embedded
     *        VERTEX_BUFFER_STATE structures.
     *
     * @param num_buffers Number of VERTEX_BUFFER_STATE structures that will
     *                     immediately follow this header in the ring (1-33).
     */
    [[nodiscard]] static constexpr VERTEX_BUFFERS_HEADER create(u32 num_buffers) {
        VERTEX_BUFFERS_HEADER cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_3DSTATE_VERTEX_BUFFERS;
        cmd.dword_length = (4 * num_buffers) - 1;
        return cmd;
    }
};

static_assert(sizeof(VERTEX_BUFFERS_HEADER) == 4, "VERTEX_BUFFERS_HEADER must be 32 bits");

#endif  // VESPERAOS_CMD_VERTEX_BUFFERS_H
