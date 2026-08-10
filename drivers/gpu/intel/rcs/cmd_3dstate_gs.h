// cmd_3dstate_gs.h
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

#ifndef VESPERAOS_CMD_3DSTATE_GS_H
#define VESPERAOS_CMD_3DSTATE_GS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_GS command (10 DWords).
 *
 * Controls the Geometry Shader (GS) stage hardware in the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 113-123 (3DSTATE_GS)
 */
union STATE_GS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_GS = 0x11 };

    enum DispatchMode : u32 {
        DISPATCH_SINGLE        = 0x0,
        DISPATCH_DUAL_INSTANCE = 0x1,
        DISPATCH_DUAL_OBJECT   = 0x2,
        DISPATCH_SIMD8         = 0x3
    };

    enum ReorderMode : u32 { REORDER_LEADING = 0x0, REORDER_TRAILING = 0x1 };

    enum ControlDataFormat : u32 { CONTROL_DATA_FORMAT_CUT = 0x0, CONTROL_DATA_FORMAT_SID = 0x1 };

    enum VectorMaskSelect : u32 { VME_DMASK = 0x0, VME_VMASK = 0x1 };

    enum FloatingPointMode : u32 { FP_MODE_IEEE754 = 0x0, FP_MODE_ALTERNATE = 0x1 };

    enum ThreadDispatchPriority : u32 { PRIORITY_NORMAL = 0x0, PRIORITY_HIGH = 0x1 };

    struct {
        // DWord 0
        u32 dword_length : 8;    ///< [7:0]   Default: 0x8 (Excludes DWord 0,1)
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x11 (3DSTATE_GS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // DWord 1..2
        u64 reserved1_0 : 6;                ///< [5:0]   MBZ
        u64 kernel_start_pointer : 58;      ///< [63:6]  InstructionBaseOffset[63:6] Kernel

        // DWord 3
        u32 expected_vertex_count : 6;           ///< [5:0]   U6; Expected vertices per input object [1,32]
        u32 reserved3_6 : 1;                     ///< [6]     MBZ
        u32 software_exception_enable : 1;       ///< [7]     Loaded into EU CR0.1[13]
        u32 reserved3_8 : 3;                     ///< [10:8]  MBZ
        u32 mask_stack_exception_enable : 1;     ///< [11]    Loaded into EU CR0.1[11]
        u32 accesses_uav : 1;                    ///< [12]    Must be set if GS accesses UAV
        u32 illegal_opcode_exception_enable : 1;  ///< [13]    Loaded into EU CR0.1[12]
        u32 reserved3_14 : 2;                    ///< [15:14] MBZ
        u32 floating_point_mode : 1;             ///< [16]    FloatingPointMode enum
        u32 thread_dispatch_priority : 1;        ///< [17]    ThreadDispatchPriority enum
        u32 binding_table_entry_count : 8;       ///< [25:18] U8; Prefetch count for binding table entries
        u32 reserved3_26 : 1;                    ///< [26]    MBZ
        u32 sampler_count : 3;                   ///< [29:27] U3; Multiples of 4 (0=No, 1=1-4, 2=5-8...)
        u32 vector_mask_enable : 1;              ///< [30]    VectorMaskSelect enum
        u32 single_program_flow : 1;             ///< [31]    1 = Single program flow, 0 = Multiple

        // DWord 4..5
        u64 per_thread_scratch_space : 4;    ///< [3:0]   U4; power of 2 Bytes over 1K Bytes [0,11]
        u64 reserved4_4 : 6;                 ///< [9:4]   MBZ
        u64 scratch_space_base_pointer : 54; ///< [63:10] GeneralStateOffset[63:10] ScratchSpace

        // DWord 6
        u32 dispatch_grf_start_register_for_urb_data : 4;   ///< [3:0]   GRF bits [3:0]
        u32 vertex_urb_entry_read_offset : 6;               ///< [9:4]   U6, in 256-bit units [0,63]
        u32 include_vertex_handles : 1;                     ///< [10]    Include input Vertex URB handles
        u32 vertex_urb_entry_read_length : 6;               ///< [16:11] U6, in 256-bit units [0,63]
        u32 output_topology : 6;                            ///< [22:17] 3D_Prim_Topo_Type
        u32 output_vertex_size : 6;                         ///< [28:23] U6, [0,63] indicating [1,64] 16B units
        u32 dispatch_grf_start_register_for_urb_data_5 : 2; ///< [30:29] GRF bits [5:4]
        u32 reserved6_31 : 1;                               ///< [31]    MBZ

        // DWord 7
        u32 enable : 1;                     ///< [0]     1 = Enabled, 0 = Disabled (pass-through)
        u32 discard_adjacency : 1;          ///< [1]     Discard adjacent vertices
        u32 reorder_mode : 1;               ///< [2]     ReorderMode enum
        u32 hint : 1;                       ///< [3]     Passed in GS thread payload
        u32 include_primitive_id : 1;       ///< [4]     Include Primitive ID in payload R1
        u32 invocations_increment_value : 5;///< [9:5]   U5; Increment value [0,31] -> [1,32]
        u32 statistics_enable : 1;          ///< [10]    Enable GS unit-specific statistics
        u32 dispatch_mode : 2;              ///< [12:11] DispatchMode enum
        u32 default_stream_id : 2;          ///< [14:13] Default Stream ID
        u32 instance_control : 5;           ///< [19:15] U5-1; Number of instances (-1) [0,31] -> [1,32]
        u32 control_data_header_size : 4;   ///< [23:20] U4; Number of 32B units [0,8]
        u32 reserved7_24 : 2;               ///< [25:24] MBZ
        u32 reserved7_26 : 6;               ///< [31:26] MBZ

        // DWord 8
        u32 maximum_number_of_threads : 9;  ///< [8:0]   U9-1; Max active threads [7,335] -> [8,336]
        u32 reserved8_9 : 7;                ///< [15:9]  MBZ
        u32 static_output_vertex_count : 11;///< [26:16] U11; Count of output vertices
        u32 reserved8_27 : 3;               ///< [29:27] MBZ
        u32 static_output : 1;              ///< [30]    1 = Static output vertex count
        u32 control_data_format : 1;        ///< [31]    ControlDataFormat enum (0=CUT, 1=SID)

        // DWord 9
        u32 user_clip_distance_cull_test_enable_bitmask : 8; ///< [7:0]   8-bit enable bitmask
        u32 user_clip_distance_clip_test_enable_bitmask : 8; ///< [15:8]  8-bit enable bitmask
        u32 vertex_urb_entry_output_length : 5;              ///< [20:16] U5; [1,16] 256-bit increments
        u32 vertex_urb_entry_output_read_offset : 6;         ///< [26:21] U6; [0,63] 256-bit offset
        u32 reserved9_27 : 1;                                ///< [27]    MBZ
        u32 reserved9_28 : 4;                                ///< [31:28] MBZ
    } __attribute__((packed));

    u32 raw[10];

    /**
     * @brief Creates a default-initialized 3DSTATE_GS command.
     */
    [[nodiscard]] static constexpr STATE_GS create() {
        STATE_GS cmd{};
        cmd.dword_length = 0x8; // 10 DWords total - 2 = 8
        cmd.sub_opcode   = SUBOP_3DSTATE_GS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Creates a 3DSTATE_GS command that disables the Geometry Shader stage (pass-through).
     */
    [[nodiscard]] static constexpr STATE_GS create_disabled() {
        STATE_GS cmd = create();
        cmd.enable = 0; // Disable GS stage
        return cmd;
    }
};

static_assert(sizeof(STATE_GS) == 40, "STATE_GS must be 10 DWords (40 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_GS_H