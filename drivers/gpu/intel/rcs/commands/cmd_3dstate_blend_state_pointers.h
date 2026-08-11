// cmd_3dstate_blend_state_pointers.h
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

#ifndef VESPERAOS_CMD_3DSTATE_BLEND_STATE_POINTERS_H
#define VESPERAOS_CMD_3DSTATE_BLEND_STATE_POINTERS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_BLEND_STATE_POINTERS command (2 DWords).
 *
 * Sets up the pointer to the BLEND_STATE dynamic state structure (64-byte aligned),
 * relative to Dynamic State Base Address.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, p. 110 (3DSTATE_BLEND_STATE_POINTERS)
 */
union STATE_BLEND_STATE_POINTERS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_BLEND_STATE_POINTERS = 0x24 };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x24 (3DSTATE_BLEND_STATE_POINTERS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 blend_state_pointer_valid : 1;    ///< [0]     Pointer valid / state changed
        u32 reserved1_1 : 5;                  ///< [5:1]   MBZ
        u32 blend_state_pointer : 26;         ///< [31:6]  64-byte aligned offset (Dynamic State)
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_BLEND_STATE_POINTERS command.
     */
    [[nodiscard]] static constexpr STATE_BLEND_STATE_POINTERS create() {
        STATE_BLEND_STATE_POINTERS cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = SUBOP_3DSTATE_BLEND_STATE_POINTERS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_BLEND_STATE_POINTERS command pointing to a 64-byte aligned offset.
     *
     * @param offset_bytes 64-byte aligned offset relative to Dynamic State Base Address.
     */
    [[nodiscard]] static constexpr STATE_BLEND_STATE_POINTERS create_pointer(u32 offset_bytes) {
        STATE_BLEND_STATE_POINTERS cmd = create();
        cmd.blend_state_pointer = offset_bytes >> 6;
        cmd.blend_state_pointer_valid = 1;
        return cmd;
    }
};

static_assert(sizeof(STATE_BLEND_STATE_POINTERS) == 8, "STATE_BLEND_STATE_POINTERS must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_BLEND_STATE_POINTERS_H