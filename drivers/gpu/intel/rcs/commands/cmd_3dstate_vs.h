// cmd_3dstate_vs.h
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

#ifndef VESPERAOS_CMD_3DSTATE_VS_H
#define VESPERAOS_CMD_3DSTATE_VS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_VS command (9 DWords).
 *
 * This command specifies most of the state used by the Vertex Shader (VS) stage.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 258-265 (3DSTATE_VS)
 */
union STATE_VS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_VS = 0x10 };

    enum VectorMaskSelect : u32 { VME_DMASK = 0x0, VME_VMASK = 0x1 };

    enum FloatingPointMode : u32 { FP_MODE_IEEE754 = 0x0, FP_MODE_ALTERNATE = 0x1 };

    enum ThreadDispatchPriority : u32 { PRIORITY_NORMAL = 0x0, PRIORITY_HIGH = 0x1 };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x7 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x10 (3DSTATE_VS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1..2
        u64 reserved1_0 : 6;                 ///< [5:0]   MBZ
        u64 kernel_start_pointer : 58;       ///< [63:6]  InstructionBaseOffset[63:6]Kernel
                                             ///<         Ignored if VS Function Enable is DISABLED.

        // DWord 3
        u32 reserved3_0 : 7;                 ///< [6:0]   MBZ
        u32 software_exception_enable : 1;   ///< [7]     Loaded into EU CR0.1[13]
        u32 reserved3_8 : 4;                 ///< [11:8]  MBZ
        u32 accesses_uav : 1;                ///< [12]    Must be set when VS has a UAV access
        u32 illegal_opcode_exception_enable : 1; ///< [13] Loaded into EU CR0.1[12]
        u32 reserved3_14 : 2;                ///< [15:14] MBZ
        u32 floating_point_mode : 1;         ///< [16]    FloatingPointMode enum
        u32 thread_dispatch_priority : 1;    ///< [17]    ThreadDispatchPriority enum
        u32 binding_table_entry_count : 8;   ///< [25:18] U8; indicates which cache lines should be fetched
        u32 reserved3_26 : 1;                ///< [26]    MBZ
        u32 sampler_count : 3;               ///< [29:27] U3; In multiples of 4 (0=No, 1=1-4, 2=5-8...)
        u32 vector_mask_enable : 1;          ///< [30]    VectorMaskSelect enum
        u32 single_vertex_dispatch : 1;      ///< [31]    0 = Multiple, 1 = Single

        // DWord 4..5
        u64 per_thread_scratch_space : 4;    ///< [3:0]   U4, power of 2 Bytes over 1K Bytes [0,11]
        u64 reserved4_4 : 6;                 ///< [9:4]   MBZ
        u64 scratch_space_base_pointer : 54; ///< [63:10] GeneralStateOffset[63:10]ScratchSpace

        // DWord 6
        u32 reserved6_0 : 4;                     ///< [3:0]   MBZ
        u32 vertex_urb_entry_read_offset : 6;    ///< [9:4]   U6, in 256-bit units [0,63]
        u32 reserved6_10 : 1;                    ///< [10]    MBZ
        u32 vertex_urb_entry_read_length : 6;    ///< [16:11] U6, [1,63] or [0,15] if SIMD8 enabled
        u32 reserved6_17 : 3;                    ///< [19:17] MBZ
        u32 dispatch_grf_start_register_for_urb_data : 5; ///< [24:20] U5, indicating GRF [R0, R31]
        u32 reserved6_25 : 7;                    ///< [31:25] MBZ

        // DWord 7
        u32 function_enable : 1;                 ///< [0]     If disabled, VF vertices pass thru unmodified
        u32 vertex_cache_disable : 1;            ///< [1]     Controls the operation of the Vertex Cache
        u32 simd8_dispatch_enable : 1;           ///< [2]     If enabled, SIMD8 thread dispatches are performed
        u32 reserved7_3 : 6;                     ///< [8:3]   MBZ
        u32 reserved7_9 : 1;                     ///< [9]     MBZ
        u32 statistics_enable : 1;               ///< [10]    If enabled, VS stage will perform statistics gathering
        u32 reserved7_11 : 2;                    ///< [12:11] MBZ
        u32 reserved7_13 : 9;                    ///< [21:13] MBZ
        u32 reserved7_22 : 1;                    ///< [22]    MBZ
        u32 maximum_number_of_threads : 9;       ///< [31:23] U9-1 Thread count [0,335]

        // DWord 8
        u32 user_clip_distance_cull_test_enable_bitmask : 8; ///< [7:0]   U8
        u32 user_clip_distance_clip_test_enable_bitmask : 8; ///< [15:8]  U8
        u32 vertex_urb_entry_output_length : 5;              ///< [20:16] U5, in 256-bit units [1,16]
        u32 vertex_urb_entry_output_read_offset : 6;         ///< [26:21] U6, in 256-bit units [0,63]
        u32 reserved8_27 : 1;                                ///< [27]    MBZ
        u32 reserved8_28 : 4;                                ///< [31:28] MBZ
    } __attribute__((packed));

    u32 raw[9];

    /**
     * @brief Creates a default-initialized 3DSTATE_VS command.
     */
    [[nodiscard]] static constexpr STATE_VS create() {
        STATE_VS cmd{};
        cmd.dword_length = 0x7; // 9 DWords total - 2 = 7
        cmd.sub_opcode   = SUBOP_3DSTATE_VS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        cmd.statistics_enable = 1;
        return cmd;
    }
};

static_assert(sizeof(STATE_VS) == 36, "STATE_VS must be 9 DWords (36 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_VS_H