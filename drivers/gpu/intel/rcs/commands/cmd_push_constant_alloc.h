// cmd_push_constant_alloc.h
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

#ifndef VESPERAOS_CMD_PUSH_CONSTANT_ALLOC_H
#define VESPERAOS_CMD_PUSH_CONSTANT_ALLOC_H

#include <vespera/types.h>

#include "cmd_common.h"


union PUSH_CONSTANT_ALLOC;
using PUSH_CONSTANT_ALLOC_VS = PUSH_CONSTANT_ALLOC;
using PUSH_CONSTANT_ALLOC_HS = PUSH_CONSTANT_ALLOC;
using PUSH_CONSTANT_ALLOC_DS = PUSH_CONSTANT_ALLOC;
using PUSH_CONSTANT_ALLOC_GS = PUSH_CONSTANT_ALLOC;
using PUSH_CONSTANT_ALLOC_PS = PUSH_CONSTANT_ALLOC;

/**
 * @brief Generic 3DSTATE_PUSH_CONSTANT_ALLOC_* command (2 DWords / 64 bits).
 *
 * Sets up the URB configuration (offset + size) for a given stage's Push
 * Constant Buffer. The structure layout is identical for all pipeline stages
 * (VS, HS, DS, GS, PS); only the Sub-Opcode varies. Mirrors the pattern used
 * by @ref STATE_BINDING_TABLE_POINTERS.
 *
 * @note Both Constant Buffer Offset and Constant Buffer Size are in 2KB
 *       increments: offset range [0, 31] -> 0KB-31KB, size range [0, 32]
 *       -> 0KB-32KB.
 * @note The sum of Constant Buffer Offset and Constant Buffer Size must not
 *       exceed the maximum value of Constant Buffer Size (i.e. allocations
 *       across all 5 stages must not overlap and must stay within the URB's
 *       push-constant region).
 * @note The sum of the constant length programmed in the corresponding
 *       3DSTATE_CONSTANT_* command must be <= the size allocated here
 *       (including buffering for half cachelines).
 * @note 3DSTATE_CONSTANT_<stage> must be committed as state prior to shaders
 *       generating thread payloads after programming this command.
 * @note Value of 0 for Constant Buffer Size is only valid when constants are
 *       not enabled for that stage.
 * @note Commit timing: with Gather at Set Shader disabled (the common case),
 *       the corresponding 3DSTATE_CONSTANT_* command commits when
 *       3DPRIMITIVE is parsed. With Gather at Set Shader enabled, the commit
 *       point is instead the matching 3DSTATE_BINDING_TABLE_POINTER_<stage>
 *       command - in that case 3DSTATE_BINDING_TABLE_POINTER_<stage> must be
 *       reprogrammed prior to the next 3DPRIMITIVE if this command was
 *       reprogrammed in between.
 * @warning Workaround (HS/DS/GS/PS per PRM; applied here to all stages for
 *          safety): this command must be followed by a PIPE_CONTROL with the
 *          CS Stall bit set.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 158-167
 *      (3DSTATE_PUSH_CONSTANT_ALLOC_DS/GS/HS/PS/VS)
 */
union PUSH_CONSTANT_ALLOC {
    enum CommandSubOpcode : u32 {
        SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_VS = 0x12,
        SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_HS = 0x13,
        SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_DS = 0x14,
        SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_GS = 0x15,
        SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_PS = 0x16,
    };

    struct {
        // ====================================================================
        // DWord 0
        // ====================================================================
        u32 dword_length : 8;   ///< [7:0]   Default: 0x0 (Excludes DWords 0,1)
        u32 reserved0_8  : 8;   ///< [15:8]  MBZ
        u32 sub_opcode   : 8;   ///< [23:16] CommandSubOpcode enum (e.g. 0x12 for VS)
        u32 opcode       : 3;   ///< [26:24] Default: 0x1  (3DSTATE_NONPIPELINED)
        u32 sub_type     : 2;   ///< [28:27] Default: 0x3  (GFXPIPE_3D)
        u32 command_type : 3;   ///< [31:29] Default: 0x3  (GFXPIPE)

        // ====================================================================
        // DWord 1
        // ====================================================================
        u32 constant_buffer_size   : 6;  ///< [5:0]   Size in 2KB increments [0, 32] (0KB - 32KB)
        u32 reserved1_6            : 10; ///< [15:6]  MBZ
        u32 constant_buffer_offset : 5;  ///< [20:16] Offset into URB in 2KB increments [0, 31] (0KB - 31KB)
        u32 reserved1_21           : 11; ///< [31:21] MBZ
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Internal helper to create a correctly initialized command for a specific stage.
     *
     * @param stage      Which stage's push constant buffer to configure.
     * @param offset_2kb Offset of the constant buffer into the URB (in 2KB increments, 0-31).
     * @param size_2kb   Size of the constant buffer (in 2KB increments, 0-32).
     */
    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC create_generic(
        CommandSubOpcode stage,
        u32 offset_2kb,
        u32 size_2kb
    ) {
        PUSH_CONSTANT_ALLOC cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_NONPIPELINED;
        cmd.sub_opcode = stage;
        cmd.dword_length = 0x0;
        cmd.constant_buffer_offset = offset_2kb;
        cmd.constant_buffer_size = size_2kb;
        return cmd;
    }

    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC_VS create_vs(u32 offset_2kb, u32 size_2kb) {
        return create_generic(SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_VS, offset_2kb, size_2kb);
    }

    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC_HS create_hs(u32 offset_2kb, u32 size_2kb) {
        return create_generic(SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_HS, offset_2kb, size_2kb);
    }

    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC_DS create_ds(u32 offset_2kb, u32 size_2kb) {
        return create_generic(SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_DS, offset_2kb, size_2kb);
    }

    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC_GS create_gs(u32 offset_2kb, u32 size_2kb) {
        return create_generic(SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_GS, offset_2kb, size_2kb);
    }

    [[nodiscard]] static constexpr PUSH_CONSTANT_ALLOC_PS create_ps(u32 offset_2kb, u32 size_2kb) {
        return create_generic(SUBOP_3DSTATE_PUSH_CONSTANT_ALLOC_PS, offset_2kb, size_2kb);
    }
};


static_assert(sizeof(PUSH_CONSTANT_ALLOC) == 8, "PUSH_CONSTANT_ALLOC must be exactly 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_PUSH_CONSTANT_ALLOC_H