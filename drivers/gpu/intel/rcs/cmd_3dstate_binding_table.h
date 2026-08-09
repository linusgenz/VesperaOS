// cmd_3dstate_binding_table.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 08.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_BINDING_TABLE_POINTERS_H
#define VESPERAOS_CMD_3DSTATE_BINDING_TABLE_POINTERS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief BINDING_TABLE_STATE structure (1 DWord).
 *
 * The binding table binds surfaces to logical resource indices used by shaders
 * and other compute engine kernels. It is stored as an array of up to 256 elements,
 * each of which contains one dword. The first element is aligned to a 64-byte boundary.
 *
 * @note Binding table indexes beyond 256 will automatically be mapped to entry 0
 *       by the HW, except any messages which support the special
 *       indexes 240 through 255, inclusive.
 *
 * @see BSpec (BINDING_TABLE_STATE)
 */
union BINDING_TABLE_STATE {
    struct {
        // DWord 0
        u32 reserved0_0 : 6;              ///< [5:0]  MBZ
        u32 surface_state_pointer : 26;   ///< [31:6] SurfaceStateOffset[31:6]
        ///<        This 64-byte aligned address points to a
        ///<        surface state block. This pointer is relative
        ///<        to the Surface State Base Address.
    } __attribute__((packed));

    u32 raw[1];

    /**
     * @brief Creates a default-initialized BINDING_TABLE_STATE (Default Value: 0x00000000).
     */
    [[nodiscard]] static constexpr BINDING_TABLE_STATE create() {
        BINDING_TABLE_STATE state{};
        return state;
    }
};

static_assert(sizeof(BINDING_TABLE_STATE) == 4, "BINDING_TABLE_STATE must be 1 DWord (4 bytes)");

/**
 * @brief Generic 3DSTATE_BINDING_TABLE_POINTERS command (2 DWords).
 *
 * This command is used to define the location of the fixed functions'
 * BINDING_TABLE_STATE. The structure layout is identical for all pipeline
 * stages (VS, HS, DS, GS, PS); only the Sub-Opcode varies.
 *
 * @note The base and alignment of the pointer differ depending on whether
 *       the HW Binding Table Pool is enabled and the alignment field setting
 *       (32B, 64B, or 256B aligned).
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, p. 25 (3DSTATE_BINDING_TABLE_POINTERS_PS)
 */
union STATE_BINDING_TABLE_POINTERS {
    enum CommandSubOpcode : u32 {
        SUBOP_3DSTATE_BINDING_TABLE_POINTERS_VS = 0x26,
        SUBOP_3DSTATE_BINDING_TABLE_POINTERS_HS = 0x27,
        SUBOP_3DSTATE_BINDING_TABLE_POINTERS_DS = 0x28,
        SUBOP_3DSTATE_BINDING_TABLE_POINTERS_GS = 0x29,
        SUBOP_3DSTATE_BINDING_TABLE_POINTERS_PS = 0x2A,
    };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x0 (2 DWords - 2)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] CommandSubOpcode enum (e.g., 0x2A for PS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 reserved1_0 : 5;              ///< [4:0]   MBZ
        u32 pointer_to_binding_table : 11; ///< [15:5]  SurfaceStateOffset[15:5] or [16:6]
                                          ///<         depending on Binding Table Pool config
        u32 reserved1_16 : 16;            ///< [31:16] MBZ
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Internal helper to create a correctly initialized command for a specific stage.
     */
    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_generic(CommandSubOpcode stage) {
        STATE_BINDING_TABLE_POINTERS cmd{};
        cmd.dword_length = 0x0; // 2 DWords total - 2 = 0
        cmd.sub_opcode   = stage;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_vs() {
        return create_generic(SUBOP_3DSTATE_BINDING_TABLE_POINTERS_VS);
    }

    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_hs() {
        return create_generic(SUBOP_3DSTATE_BINDING_TABLE_POINTERS_HS);
    }

    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_ds() {
        return create_generic(SUBOP_3DSTATE_BINDING_TABLE_POINTERS_DS);
    }

    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_gs() {
        return create_generic(SUBOP_3DSTATE_BINDING_TABLE_POINTERS_GS);
    }

    [[nodiscard]] static constexpr STATE_BINDING_TABLE_POINTERS create_ps() {
        return create_generic(SUBOP_3DSTATE_BINDING_TABLE_POINTERS_PS);
    }
};

static_assert(sizeof(STATE_BINDING_TABLE_POINTERS) == 8, "STATE_BINDING_TABLE_POINTERS must be 2 DWords (8 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_BINDING_TABLE_POINTERS_H