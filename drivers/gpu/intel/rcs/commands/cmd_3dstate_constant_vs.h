// cmd_3dstate_constant_vs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_CONSTANT_VS_H
#define VESPERAOS_CMD_3DSTATE_CONSTANT_VS_H

#include <vespera/types.h>

#include "cmd_common.h"
#include <gpu/intel/rcs/state/3dstate_constant_body.h>

/**
 * @brief 3DSTATE_CONSTANT_VS command structure (11 DWords / 352 bits).
 *
 * Sets pointers to push constants for the Vertex Shader (VS) stage. Loaded
 * constant data is placed into the VS Push Constant Buffer (PCB). DWords
 * 1..10 are the shared @ref THREE_DSTATE_CONSTANT_BODY payload, common to
 * 3DSTATE_CONSTANT_VS/HS/DS/GS.
 *
 * @note The command is committed to the shader unit only when the corresponding
 *       3DSTATE_BINDING_TABLE_POINTERS_VS is parsed.
 * @note Total length is 11 DWords (DWord length field = 11 - 2 = 9).
 * @warning Workaround: never commit a 3DSTATE_CONSTANT_* with buffer 3 read
 *          length == 0 immediately followed (without a 3D engine flush) by
 *          one with buffer 0 read length != 0. This driver avoids the hazard
 *          by always forcing buffer 3 to a non-zero read length.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 50-51 (3DSTATE_CONSTANT_VS)
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 3-5 (3DSTATE_CONSTANT(Body))
 */
union CMD_3DSTATE_CONSTANT_VS {
    enum CommandSubOpcode : u32 {
        SUBOP_3DSTATE_CONSTANT_VS = 0x15,
    };

    struct {
        // ====================================================================
        // DWord 0
        // ====================================================================
        u32 dword_length : 8; ///< [7:0]   Default: 0x9 (11 DWords - 2)
        u32 mocs         : 7; ///< [14:8]  Memory Object Control State
        u32 reserved0_15 : 1; ///< [15]    MBZ
        u32 sub_opcode   : 8; ///< [23:16] Default: 0x15 (3DSTATE_CONSTANT_VS)
        u32 opcode       : 3; ///< [26:24] Default: 0x0  (3DSTATE_PIPELINED)
        u32 sub_type     : 2; ///< [28:27] Default: 0x3  (GFXPIPE_3D)
        u32 command_type : 3; ///< [31:29] Default: 0x3  (GFXPIPE)

        // ====================================================================
        // DWord 1..10 - Shared Constant Body (Read Lengths + Buffer Pointers)
        // ====================================================================
        THREE_DSTATE_CONSTANT_BODY body; ///< DW1-DW10, see 3DSTATE_CONSTANT(Body)
    } __attribute__((packed));

    u32 raw[11];

    /**
     * @brief Helper to initialize standard command header fields.
     */
    [[nodiscard]] static constexpr CMD_3DSTATE_CONSTANT_VS make_header(u32 mocs_val) {
        CMD_3DSTATE_CONSTANT_VS cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_3DSTATE_CONSTANT_VS;
        cmd.mocs = mocs_val;
        cmd.dword_length = 0x9; // 11 DWords - 2
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CONSTANT_VS command configuring Buffer 0 for VS push constants.
     *
     * @param buffer0_address Byte address of Buffer 0 in Dynamic/General State (32-byte aligned).
     * @param read_length_32b Length of constant data in 32-byte (256-bit) units (0-64).
     * @param mocs_val        Memory Object Control State (MOCS).
     */
    [[nodiscard]] static constexpr CMD_3DSTATE_CONSTANT_VS create_buffer0(
        u64 buffer0_address,
        u16 read_length_32b,
        u32 mocs_val = 0
    ) {
        CMD_3DSTATE_CONSTANT_VS cmd = make_header(mocs_val);
        cmd.body = THREE_DSTATE_CONSTANT_BODY::create_buffer0(buffer0_address, read_length_32b);

        // Forces Buffer 3 to have a non-zero read length to prevent GPU hangs
        // when transitioning from a committed buffer-3-disabled state to a
        // committed buffer-0-enabled state without an intervening flush.
        // @see IHD-OS-KBL-Vol 2a-1.17 page 50 (Workaround)
        if (read_length_32b > 0) {
            cmd.body.buffer3_read_length = 1;
            cmd.body.buffer3_pointer_low  = static_cast<u32>(buffer0_address & 0xFFFFFFFF);
            cmd.body.buffer3_pointer_high = static_cast<u32>(buffer0_address >> 32);
        }

        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CONSTANT_VS command configuring Buffer 1.
     */
    [[nodiscard]] static constexpr CMD_3DSTATE_CONSTANT_VS create_buffer1(
        u64 buffer1_address,
        u16 read_length_32b,
        u32 mocs_val = 0
    ) {
        CMD_3DSTATE_CONSTANT_VS cmd = make_header(mocs_val);
        cmd.body = THREE_DSTATE_CONSTANT_BODY::create_buffer1(buffer1_address, read_length_32b);

        if (read_length_32b > 0) {
            cmd.body.buffer3_read_length = 1;
            cmd.body.buffer3_pointer_low  = static_cast<u32>(buffer1_address & 0xFFFFFFFF);
            cmd.body.buffer3_pointer_high = static_cast<u32>(buffer1_address >> 32);
        }
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CONSTANT_VS command configuring Buffer 2.
     */
    [[nodiscard]] static constexpr CMD_3DSTATE_CONSTANT_VS create_buffer2(
        u64 buffer2_address,
        u16 read_length_32b,
        u32 mocs_val = 0
    ) {
        CMD_3DSTATE_CONSTANT_VS cmd = make_header(mocs_val);
        cmd.body = THREE_DSTATE_CONSTANT_BODY::create_buffer2(buffer2_address, read_length_32b);

        if (read_length_32b > 0) {
            cmd.body.buffer3_read_length = 1;
            cmd.body.buffer3_pointer_low  = static_cast<u32>(buffer2_address & 0xFFFFFFFF);
            cmd.body.buffer3_pointer_high = static_cast<u32>(buffer2_address >> 32);
        }
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_CONSTANT_VS command configuring Buffer 3.
     */
    [[nodiscard]] static constexpr CMD_3DSTATE_CONSTANT_VS create_buffer3(
        u64 buffer3_address,
        u16 read_length_32b,
        u32 mocs_val = 0
    ) {
        CMD_3DSTATE_CONSTANT_VS cmd = make_header(mocs_val);
        cmd.body = THREE_DSTATE_CONSTANT_BODY::create_buffer3(buffer3_address, read_length_32b);
        return cmd;
    }
};

static_assert(sizeof(CMD_3DSTATE_CONSTANT_VS) == 44, "CMD_3DSTATE_CONSTANT_VS must be exactly 11 DWords (44 bytes)");
static_assert(sizeof(THREE_DSTATE_CONSTANT_BODY) == 40, "Constant body must be exactly 10 DWords (40 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_CONSTANT_VS_H
