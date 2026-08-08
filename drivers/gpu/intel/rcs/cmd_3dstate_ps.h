// cmd_3dstate_ps.h
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

#ifndef VESPERAOS_CMD_3DSTATE_PS_H
#define VESPERAOS_CMD_3DSTATE_PS_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief 3DSTATE_PS command (12 DWords).
 *
 * Configures the Pixel Shader thread dispatcher: up to three kernel start
 * pointers, dispatch width enables, GRF payload layout, and scratch space.
 * Lives directly in the command stream — unlike SF_CLIP_VIEWPORT or
 * CC_VIEWPORT, which are Dynamic State Base Address-relative structures
 * only pointed at from the stream.
 *
 * IMPORTANT — C++ bitfield declaration order determines actual bit
 * placement, NOT the doc comments. Every field below is declared
 * LSB-first within its DWord to match the PRM's bit numbering; a previous
 * revision of this file declared DWord 7 MSB-first (by PRM bit number),
 * which put every field at the wrong physical bit position despite
 * correct-looking comments — verified by compiling and inspecting the raw
 * value. Keep new fields in ascending bit order when editing this struct.
 *
 * @note Kernel Start Pointer 0 is only meaningful if PS Function Enable
 *       (elsewhere in pipeline state) is ENABLED; Kernel Start Pointer 1
 *       and 2 are only fetched if the corresponding dispatch-width enable
 *       bit is set and HW selects that kernel slot.
 * @note If [Maximum Number of Threads Per PSD] changes between 3DPRIMITIVE
 *       commands, a PIPE_CONTROL command with Stall at Pixel Scoreboard
 *       set is required.
 * @note [Push Constant Enable] must track whether the sum of the PS
 *       Constant Buffer Read Length fields in 3DSTATE_CONSTANT_PS is
 *       nonzero.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 147-153 (3DSTATE_PS)
 */
union STATE_PS {
    enum CommandSubOpcode : u32 { SUBOP_3DSTATE_PS = 0x20 };

    enum SingleProgramFlow : u32 { SPF_MULTIPLE = 0x0, SPF_SINGLE = 0x1 };

    enum VectorMaskSelect : u32 { VME_DMASK = 0x0, VME_VMASK = 0x1 };

    enum SamplerCount : u32 {
        SAMPLERS_NONE = 0x0,
        SAMPLERS_1_4 = 0x1,
        SAMPLERS_5_8 = 0x2,
        SAMPLERS_9_12 = 0x3,
        SAMPLERS_13_16 = 0x4,
    };

    enum SinglePrecisionDenormalMode : u32 { DENORM_FLUSHED_TO_ZERO = 0x0, DENORM_RETAINED = 0x1 };

    enum RoundingMode : u32 {
        ROUND_RTNE = 0x0,  ///< Round to Nearest Even
        ROUND_RU = 0x1,    ///< Round toward +infinity
        ROUND_RD = 0x2,    ///< Round toward -infinity
        ROUND_RTZ = 0x3,   ///< Round toward zero
    };

    enum FloatingPointMode : u32 { FP_MODE_IEEE754 = 0x0, FP_MODE_ALTERNATE = 0x1 };

    enum ThreadDispatchPriority : u32 { PRIORITY_NORMAL = 0x0, PRIORITY_HIGH = 0x1 };

    enum PositionOffsetSelect : u32 {
        POSOFFSET_NONE = 0x0,
        POSOFFSET_CENTROID = 0x2,
        POSOFFSET_SAMPLE = 0x3,  ///< Requires MSDISPMODE_PERSAMPLE
    };

    enum RenderTargetResolveType : u32 {
        RESOLVE_DISABLED = 0x0,
        RESOLVE_PARTIAL = 0x1,
        RESOLVE_FULL = 0x3,
    };

    struct {
        // ====================================================================
        // DWord 0
        // ====================================================================
        u32 dword_length : 8;    ///< [7:0]   Default: 0xA, Total Length - 2
        u32 reserved0_8 : 8;     ///< [15:8]  MBZ
        u32 sub_opcode : 8;      ///< [23:16] Default: 0x20 (3DSTATE_PS)
        u32 opcode : 3;          ///< [26:24] Default: 0x0 (3DSTATE_PIPELINED)
        u32 sub_type : 2;        ///< [28:27] Default: 0x3 (GFXPIPE_3D)
        u32 command_type : 3;    ///< [31:29] Default: 0x3 (GFXPIPE)

        // ====================================================================
        // DWord 1..2: Kernel Start Pointer 0
        // ====================================================================
        u64 kernel_start_pointer_0_reserved : 6;  ///< [5:0]   MBZ
        u64 kernel_start_pointer_0 : 58;          ///< [63:6]  InstructionBaseOffset[63:6]Kernel,
                                                   ///<         relative to Instruction Base Address.
                                                   ///<         Use set_kernel_start_pointer_0() below
                                                   ///<         (takes a byte offset, shifts for you).


        // ====================================================================
        // DWord 3
        // ====================================================================
        u32 reserved4_0 : 7;                        ///< [6:0]   MBZ
        u32 software_exception_enable : 1;          ///< [7]     Loaded into EU CR0.1[13]
        u32 reserved4_8 : 3;                        ///< [10:8]  MBZ
        u32 mask_stack_exception_enable : 1;        ///< [11]    Loaded into EU CR0.1[12]
        u32 reserved4_12 : 1;                       ///< [12]    MBZ
        u32 illegal_opcode_exception_enable : 1;    ///< [13]    Loaded into EU CR0.1[12]
        u32 rounding_mode : 2;                      ///< [15:14] RoundingMode enum
        u32 floating_point_mode : 1;                ///< [16]    FloatingPointMode enum
        u32 thread_dispatch_priority : 1;           ///< [17]    ThreadDispatchPriority enum
        u32 binding_table_entry_count : 8;          ///< [25:18] U8; ignored if PS Function Enable disabled
        u32 single_precision_denormal_mode : 1;     ///< [26]    SinglePrecisionDenormalMode enum
        u32 sampler_count : 3;                      ///< [29:27] SamplerCount enum
        u32 vector_mask_enable : 1;                 ///< [30]    VectorMaskSelect enum
        u32 reserved4_31 : 1;                       ///< [31]    MBZ

        // ====================================================================
        // DWord 4..5: Scratch Space Base Pointer
        // ====================================================================
        u64 per_thread_scratch_space : 4;    ///< [3:0]   U4, [1K,2M] bytes in powers of 2
        u64 reserved5_4 : 6;                 ///< [9:4]   MBZ
        u64 scratch_space_base_pointer : 54; ///< [63:10] GeneralStateOffset[63:10]ScratchSpace,
                                              ///<         1K-byte aligned, relative to General
                                              ///<         State Base Address.

        // ====================================================================
        // DWord 6
        // ====================================================================
        u32 dispatch_8_pixel_enable : 1;         ///< [0]    2 subspans/payload (SIMD8)
        u32 dispatch_16_pixel_enable : 1;        ///< [1]    4 subspans/payload (SIMD16)
        u32 dispatch_32_pixel_enable : 1;        ///< [2]    8 subspans/payload (SIMD32)
        u32 position_xy_offset_select : 2;       ///< [4:3]  PositionOffsetSelect enum
        u32 reserved7_5 : 1;                     ///< [5]    MBZ
        u32 render_target_resolve_type : 2;      ///< [7:6]  RenderTargetResolveType enum
        u32 render_target_fast_clear_enable : 1; ///< [8]    Requires BTI==0, RENDER_SURFACE_STATE != NULL
        u32 reserved7_9 : 2;                     ///< [10:9] MBZ
        u32 push_constant_enable : 1;            ///< [11]   Must match nonzero PS Constant Buffer
                                                  ///<        Read Length sum in 3DSTATE_CONSTANT_PS
        u32 reserved7_12 : 10;                   ///< [21:12] MBZ (includes bit 22 per PRM 22:12 reserved span)
        u32 maximum_number_of_threads : 9;       ///< [31:23] U8-1, [2,64] threads/PSD

        // ====================================================================
        // DWord 7 Dispatch GRF Start Registers for Constant/Setup Data
        // ====================================================================
        u32 dispatch_grf_start_2 : 7;    ///< [6:0]   GRF start for kernel[2]
        u32 reserved8_7 : 1;             ///< [7]     MBZ
        u32 dispatch_grf_start_1 : 7;    ///< [14:8]  GRF start for kernel[1]
        u32 reserved8_15 : 1;            ///< [15]    MBZ
        u32 dispatch_grf_start_0 : 7;    ///< [22:16] GRF start for kernel[0]
        u32 reserved8_23 : 9;            ///< [31:23] MBZ

        // ====================================================================
        // DWord 8..9: Kernel Start Pointer 1
        // ====================================================================
        u64 kernel_start_pointer_1_reserved : 6;  ///< [5:0]   MBZ
        u64 kernel_start_pointer_1 : 58;          ///< [63:6]  InstructionBaseOffset[63:6]Kernel

        // ====================================================================
        // DWord 10..11: Kernel Start Pointer 2
        // ====================================================================
        u64 kernel_start_pointer_2_reserved : 6;  ///< [5:0]   MBZ
        u64 kernel_start_pointer_2 : 58;          ///< [63:6]  InstructionBaseOffset[63:6]Kernel
    } __attribute__((packed));

    u32 raw[12];

    /**
     * @brief Creates a default-initialized 3DSTATE_PS command.
     */
    [[nodiscard]] static constexpr STATE_PS create() {
        STATE_PS cmd{};
        cmd.dword_length = 0xA; // 12 DWords total - 2 = 10
        cmd.sub_opcode   = SUBOP_3DSTATE_PS;
        cmd.opcode       = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_type     = GFXPIPE_3D;
        cmd.command_type = CMD_GFXPIPE;
        return cmd;
    }

    /**
     * @brief Sets Kernel Start Pointer 0 from a byte offset relative to
     *        Instruction Base Address (must be 64-byte aligned).
     */
    constexpr void set_kernel_start_pointer_0(u64 byte_offset) {
        kernel_start_pointer_0 = byte_offset >> 6;
    }

    /// @brief Same as set_kernel_start_pointer_0() but for kernel slot 1.
    constexpr void set_kernel_start_pointer_1(u64 byte_offset) {
        kernel_start_pointer_1 = byte_offset >> 6;
    }

    /// @brief Same as set_kernel_start_pointer_0() but for kernel slot 2.
    constexpr void set_kernel_start_pointer_2(u64 byte_offset) {
        kernel_start_pointer_2 = byte_offset >> 6;
    }
};

static_assert(sizeof(STATE_PS) == 48, "STATE_PS must be 12 DWords (48 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_PS_H


