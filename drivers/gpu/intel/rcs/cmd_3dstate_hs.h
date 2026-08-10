// cmd_3dstate_hs.h
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

#ifndef VESPERAOS_CMD_3DSTATE_HS_H
#define VESPERAOS_CMD_3DSTATE_HS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_HS command (9 DWords).
 *
 * Controls the Hull Shader (HS) stage hardware in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 127-133 (3DSTATE_HS)
 */
union STATE_HS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_HS = 0x1B };

    enum DispatchMode : u32 {
        DISPATCH_SINGLE_PATCH = 0x0,
        DISPATCH_DUAL_PATCH   = 0x1,
        DISPATCH_8_PATCH      = 0x2
    };

    enum VectorMaskSelect : u32 { VME_DMASK = 0x0, VME_VMASK = 0x1 };

    enum FloatingPointMode : u32 { FP_MODE_IEEE754 = 0x0, FP_MODE_ALTERNATE = 0x1 };

    enum ThreadDispatchPriority : u32 { PRIORITY_NORMAL = 0x0, PRIORITY_HIGH = 0x1 };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x7 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x1B (3DSTATE_HS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1
        u32 reserved1_0 : 12;                    ///< [11:0]  MBZ
        u32 software_exception_enable : 1;      ///< [12]    Loaded into EU CR0.1[13]
        u32 illegal_opcode_exception_enable : 1; ///< [13]    Loaded into EU CR0.1[12]
        u32 reserved1_14 : 2;                   ///< [15:14] MBZ
        u32 floating_point_mode : 1;            ///< [16]    FloatingPointMode enum
        u32 thread_dispatch_priority : 1;       ///< [17]    ThreadDispatchPriority enum
        u32 binding_table_entry_count : 8;      ///< [25:18] U8; Prefetch count for binding table entries
        u32 reserved1_26 : 1;                   ///< [26]    MBZ
        u32 sampler_count : 3;                  ///< [29:27] U3; Multiples of 4 (0=No, 1=1-4, 2=5-8...)
        u32 reserved1_30 : 2;                   ///< [31:30] MBZ

        // DWord 2
        u32 instance_count : 4;             ///< [3:0]   U4-1; Number of threads (-1) per patch [0,15]
        u32 reserved2_4 : 4;                ///< [7:4]   MBZ
        u32 maximum_number_of_threads : 9;  ///< [16:8]  U9-1; Max active threads [0,335] -> [1,336]
        u32 reserved2_17 : 12;              ///< [28:17] MBZ
        u32 statistics_enable : 1;          ///< [29]    Enable HS unit-specific statistics
        u32 reserved2_30 : 1;               ///< [30]    MBZ
        u32 enable : 1;                     ///< [31]    1 = Enabled, 0 = Disabled (pass-through)

        // DWord 3..4
        u64 reserved3_0 : 6;                ///< [5:0]   MBZ
        u64 kernel_start_pointer : 58;      ///< [63:6]  InstructionBaseOffset[63:6] Kernel

        // DWord 5..6
        u64 per_thread_scratch_space : 4;   ///< [3:0]   U4; power of 2 Bytes over 1K Bytes [0,11]
        u64 reserved5_4 : 6;                ///< [9:4]   MBZ
        u64 scratch_space_base_pointer : 54;///< [63:10] GeneralStateOffset[63:10] ScratchSpace

        // DWord 7
        u32 include_primitive_id : 1;                     ///< [0]     Include Primitive ID in payload R1
        u32 reserved7_1 : 3;                              ///< [3:1]   MBZ
        u32 vertex_urb_entry_read_offset : 6;             ///< [9:4]   U6, in 256-bit units [0,63]
        u32 reserved7_10 : 1;                             ///< [10]    MBZ
        u32 vertex_urb_entry_read_length : 6;             ///< [16:11] U6, in 256-bit units [0,63]
        u32 dispatch_mode : 2;                            ///< [18:17] DispatchMode enum
        u32 dispatch_grf_start_register_for_urb_data : 5; ///< [23:19] U5, GRF bits [4:0]
        u32 include_vertex_handles : 1;                   ///< [24]    Include input Vertex URB handles
        u32 accesses_uav : 1;                             ///< [25]    Must be set if HS accesses UAV
        u32 vector_mask_enable : 1;                       ///< [26]    VectorMaskSelect enum
        u32 single_program_flow : 1;                      ///< [27]    1 = SIMDnx1, 0 = SIMDnxm (m>1)
        u32 dispatch_grf_start_register_for_urb_data_5 : 1; ///< [28] GRF bit [5]
        u32 reserved7_29 : 3;                             ///< [31:29] MBZ

        // DWord 8
        u32 reserved8_0 : 32;               ///< [31:0]  MBZ
    } __attribute__((packed));

    u32 raw[9];

    /**
     * @brief Creates a default-initialized 3DSTATE_HS command.
     */
    [[nodiscard]] static constexpr STATE_HS create() {
        STATE_HS cmd{};
        cmd.dword_length = 0x7; // 9 DWords total - 2 = 7
        cmd.sub_opcode   = SUBOP_3DSTATE_HS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_HS command that disables the Hull Shader stage (pass-through).
     */
    [[nodiscard]] static constexpr STATE_HS create_disabled() {
        STATE_HS cmd = create();
        cmd.enable = 0; // Disable HS stage
        return cmd;
    }
};

static_assert(sizeof(STATE_HS) == 36, "STATE_HS must be 9 DWords (36 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_HS_H