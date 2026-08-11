// cmd_3dstate_urb.h
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

#ifndef VESPERAOS_CMD_3DSTATE_URB_H
#define VESPERAOS_CMD_3DSTATE_URB_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief Sub-Opcodes for 3DSTATE_URB_* commands.
 */
enum UrbSubOpcode : u8 {
    SUBOP_3DSTATE_URB_VS = 0x30,
    SUBOP_3DSTATE_URB_HS = 0x31,
    SUBOP_3DSTATE_URB_DS = 0x32,
    SUBOP_3DSTATE_URB_GS = 0x33,
};

/**
 * @brief Generic 3DSTATE_URB_ Stage Allocation command (2 DWords).
 *
 * Used for programming URB allocation state across VS, HS, DS, and GS pipeline stages.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 232-238 (3DSTATE_URB_DS/GS/HS/VS)
 */
template <UrbSubOpcode SubOpcode>
union STATE_URB_STAGE {
    enum CommandSubOpcode : u32 { SUBOP = SubOpcode };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: SubOpcode
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 number_of_urb_entries : 16;    ///< [15:0]  U16; Specifies the number of URB entries used
        u32 urb_entry_allocation_size : 9; ///< [24:16] U9-1; Length of each URB entry in 512-bit units
        u32 urb_starting_address : 7;      ///< [31:25] U7; Offset from start of URB memory in multiples of 8 KB
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized 3DSTATE_URB command for the specified stage.
     */
    [[nodiscard]] static constexpr STATE_URB_STAGE create() {
        STATE_URB_STAGE cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = SubOpcode;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }
};

// Aliases for each pipeline stage
using STATE_URB_VS = STATE_URB_STAGE<SUBOP_3DSTATE_URB_VS>;
using STATE_URB_HS = STATE_URB_STAGE<SUBOP_3DSTATE_URB_HS>;
using STATE_URB_DS = STATE_URB_STAGE<SUBOP_3DSTATE_URB_DS>;
using STATE_URB_GS = STATE_URB_STAGE<SUBOP_3DSTATE_URB_GS>;

static_assert(sizeof(STATE_URB_VS) == 8, "STATE_URB_VS must be 2 DWords (8 bytes)");
static_assert(sizeof(STATE_URB_HS) == 8, "STATE_URB_HS must be 2 DWords (8 bytes)");
static_assert(sizeof(STATE_URB_DS) == 8, "STATE_URB_DS must be 2 DWords (8 bytes)");
static_assert(sizeof(STATE_URB_GS) == 8, "STATE_URB_GS must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_URB_H