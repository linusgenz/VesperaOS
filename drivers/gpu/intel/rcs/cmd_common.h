// cmd_common.h
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

#ifndef VESPERAOS_CMD_COMMON_H
#define VESPERAOS_CMD_COMMON_H

#include <vespera/types.h>

/// Top-level Command Type field [31:29], shared by all GFXPIPE commands.
enum CommandType : u32 { CMD_GFXPIPE = 0x3 };

/// Command SubType field [28:27].
enum CommandSubType : u32 { GFXPIPE_3D = 0x3, GFXPIPE_SINGLE_DW = 0x1 };

/// Opcode field for GFXPIPE_3D command subtype
/// 3D Command Opcode field [26:24] — distinguishes pipelined vs.
/// non-pipelined 3DSTATE commands, GPGPU, 3D_CONTROL, etc.
enum CommandOpcode3D : u32 {
    OPCODE_3DSTATE_PIPELINED = 0x0,
    OPCODE_3DSTATE_NONPIPELINED = 0x1,
    OPCODE_3DCONTROL = 0x2,
    OPCODE_3DPRIMITIVE = 0x3,
    // ...
};

/// Opcode field for GFXPIPE_SINGLE_DW command subtype
enum CommandOpcodeSingleDw : u32 {
    OPCODE_SINGLE_DW_NONPIPELINED = 0x1
};

#endif