// cmd_3dstate_ds.h
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

#ifndef VESPERAOS_CMD_3DSTATE_DS_H
#define VESPERAOS_CMD_3DSTATE_DS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_DS command (11 DWords).
 *
 * Controls the Domain Shader (DS) stage hardware in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 64-71 (3DSTATE_DS)
 */
union STATE_DS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_DS = 0x1D };

    enum DispatchMode : u32 {
        DISPATCH_SIMD4X2                    = 0x0,
        DISPATCH_SIMD8_SINGLE_PATCH         = 0x1,
        DISPATCH_SIMD8_SINGLE_OR_DUAL_PATCH = 0x2
    };

    enum VectorMaskSelect : u32 { VME_DMASK = 0x0, VME_VMASK = 0x1 };

    enum FloatingPointMode : u32 { FP_MODE_IEEE754 = 0x0, FP_MODE_ALTERNATE = 0x1 };

    enum ThreadDispatchPriority : u32 { PRIORITY_NORMAL = 0x0, PRIORITY_HIGH = 0x1 };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x9 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x1D (3DSTATE_DS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1..2
        u64 reserved1_0 : 6;                ///< [5:0]   MBZ
        u64 kernel_start_pointer : 58;      ///< [63:6]  InstructionBaseOffset[63:6] Kernel

        // DWord 3
        u32 reserved3_0 : 7;                     ///< [6:0]   MBZ
        u32 software_exception_enable : 1;       ///< [7]     Loaded into EU CR0.1[13]
        u32 reserved3_8 : 5;                     ///< [12:8]  MBZ
        u32 illegal_opcode_exception_enable : 1;  ///< [13]    Loaded into EU CR0.1[12]
        u32 accesses_uav : 1;                    ///< [14]    Must be set if DS accesses UAV
        u32 reserved3_15 : 1;                    ///< [15]    MBZ
        u32 floating_point_mode : 1;             ///< [16]    FloatingPointMode enum
        u32 thread_dispatch_priority : 1;        ///< [17]    ThreadDispatchPriority enum
        u32 binding_table_entry_count : 8;       ///< [25:18] U8; Prefetch count for binding table entries
        u32 reserved3_26 : 1;                    ///< [26]    MBZ
        u32 sampler_count : 3;                   ///< [29:27] U3; Multiples of 4 (0=No, 1=1-4, 2=5-8...)
        u32 vector_mask_enable : 1;              ///< [30]    VectorMaskSelect enum
        u32 reserved3_31 : 1;                    ///< [31]    MBZ

        // DWord 4..5
        u64 per_thread_scratch_space : 4;    ///< [3:0]   U4; power of 2 Bytes over 1K Bytes [0,11]
        u64 reserved4_4 : 6;                 ///< [9:4]   MBZ
        u64 scratch_space_base_pointer : 54; ///< [63:10] GeneralStateOffset[63:10] ScratchSpace

        // DWord 6
        u32 reserved6_0 : 4;                                ///< [3:0]   MBZ
        u32 patch_urb_entry_read_offset : 6;                ///< [9:4]   U6, in 256-bit units [0,63]
        u32 reserved6_10 : 1;                               ///< [10]    MBZ
        u32 patch_urb_entry_read_length : 7;                ///< [17:11] U7, in 256-bit units [0,64]
        u32 reserved6_18 : 2;                               ///< [19:18] MBZ
        u32 dispatch_grf_start_register_for_urb_data : 5;   ///< [24:20] GRFRegister[4:0]
        u32 reserved6_25 : 7;                               ///< [31:25] MBZ

        // DWord 7
        u32 function_enable : 1;            ///< [0]     1 = Enabled, 0 = Disabled (pass-through)
        u32 cache_disable : 1;              ///< [1]     1 = Disable DS Cache, 0 = Enable
        u32 compute_w_coordinate_enable : 1;///< [2]     1 = Compute W = 1 - (U + V) for TRI domains
        u32 dispatch_mode : 2;              ///< [4:3]   DispatchMode enum
        u32 reserved7_5 : 5;                ///< [9:5]   MBZ
        u32 statistics_enable : 1;          ///< [10]    Enable DS unit-specific statistics
        u32 reserved7_11 : 10;              ///< [20:11] MBZ
        u32 maximum_number_of_threads : 9;  ///< [29:21] U9-1; Max active threads [0,335] -> [1,336]
        u32 reserved7_30 : 1;               ///< [30]    MBZ
        u32 reserved7_31 : 1;               ///< [31]    MBZ

        // DWord 8
        u32 user_clip_distance_cull_test_enable_bitmask : 8; ///< [7:0]   8-bit enable bitmask
        u32 user_clip_distance_clip_test_enable_bitmask : 8; ///< [15:8]  8-bit enable bitmask
        u32 vertex_urb_entry_output_length : 5;              ///< [20:16] U5; [1,16] 256-bit increments
        u32 vertex_urb_entry_output_read_offset : 6;         ///< [26:21] U6; [0,63] 256-bit offset
        u32 reserved8_27 : 1;                                ///< [27]    MBZ
        u32 reserved8_28 : 4;                                ///< [31:28] MBZ

        // DWord 9..10
        u64 reserved9_0 : 6;                        ///< [5:0]   MBZ
        u64 dual_patch_kernel_start_pointer : 58;  ///< [63:6]  InstructionBaseOffset[63:6] Dual Patch Kernel
    } __attribute__((packed));

    u32 raw[11];

    /**
     * @brief Creates a default-initialized 3DSTATE_DS command.
     */
    [[nodiscard]] static constexpr STATE_DS create() {
        STATE_DS cmd{};
        cmd.dword_length = 0x9; // 11 DWords total - 2 = 9
        cmd.sub_opcode   = SUBOP_3DSTATE_DS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_DS command that disables the Domain Shader stage (pass-through).
     */
    [[nodiscard]] static constexpr STATE_DS create_disabled() {
        STATE_DS cmd = create();
        cmd.function_enable = 0; // Disable DS stage
        return cmd;
    }
};

static_assert(sizeof(STATE_DS) == 44, "STATE_DS must be 11 DWords (44 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_DS_H