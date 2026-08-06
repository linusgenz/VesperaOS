// cmd_pipe_control.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.08.26.
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

#ifndef VESPERAOS_CMD_PIPE_CONTROL_H
#define VESPERAOS_CMD_PIPE_CONTROL_H

#include <vespera/types.h>

/**
 * @brief PIPE_CONTROL Command.
 *
 * The PIPE_CONTROL command is used for synchronization, cache flushes,
 * and TLB invalidation within the 3D and GPGPU pipelines. It also allows
 * for post-sync operations such as writing immediate data, depth counts,
 * or timestamps to memory.
 *
 * @note Software must carefully manage write cache flushes (e.g., Render Target
 *       Cache, Depth Cache) along with stalling parameters like "CS Stall"
 *       to guarantee correct execution and memory consistency.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 1106–1110 (PIPE_CONTROL)
 */
union PIPE_CONTROL {
    enum PostSyncOp : u32 {
        NO_WRITE = 0b00,             ///< No write occurs as a result of this instruction
        WRITE_IMMEDIATE_DATA = 0b01, ///< Write the 64-bit Immediate Data to the Destination Address
        WRITE_PS_DEPTH_COUNT = 0b10, ///< Write the 64-bit PS_DEPTH_COUNT register to the Destination Address
        WRITE_TIMESTAMP = 0b11,      ///< Write the 64-bit TIMESTAMP register to the Destination Address
    };

    struct {
        // ====================================================================
        // DWord 0: Command Header
        // ====================================================================
        u32 dword_length : 8;        ///< [7:0]   Length excluding DW0,1 (Default: 0x4 -> 6 DWords total)
        u32 reserved0_8 : 8;         ///< [15:8]  MBZ
        u32 sub_opcode : 8;          ///< [23:16] Default: 0x00 (PIPE_CONTROL)
        u32 opcode : 3;              ///< [26:24] Default: 0x2  (3D_CONTROL)
        u32 sub_type : 2;            ///< [28:27] Default: 0x3  (GFXPIPE_3D)
        u32 type : 3;                ///< [31:29] Default: 0x3  (GFXPIPE)

        // ====================================================================
        // DWord 1: Control & Flush Flags
        // ====================================================================
        u32 depth_cache_flush_enable : 1;           ///< [0]  Flush HiZ, Stencil, Depth caches
        u32 stall_at_pixel_scoreboard : 1;          ///< [1]  Stall at the pixel scoreboard
        u32 state_cache_invalidation_enable : 1;    ///< [2]  Invalidate L1/L2 state caches
        u32 constant_cache_invalidation_enable : 1; ///< [3]  Invalidate constant cache
        u32 vf_cache_invalidation_enable : 1;       ///< [4]  Invalidate VF address based cache
        u32 dc_flush_enable : 1;                    ///< [5]  Flush L3$ portions caching DC writes
        u32 reserved1_6 : 1;                        ///< [6]  MBZ
        u32 pipe_control_flush_enable : 1;          ///< [7]  Wait for outstanding post sync ops
        u32 notify_enable : 1;                      ///< [8]  Generate Sync Completion Interrupt
        u32 indirect_state_pointers_disable : 1;    ///< [9]  Invalidate indirect state pointers
        u32 texture_cache_invalidation_enable : 1;  ///< [10] Invalidate texture caches
        u32 instruction_cache_invalidate_enable : 1;///< [11] Invalidate L1/L2 instruction caches
        u32 render_target_cache_flush_enable : 1;   ///< [12] Flush Render Target Cache
        u32 depth_stall_enable : 1;                 ///< [13] Stall pipeline at Depth Test stage
        PostSyncOp post_sync_operation : 2;         ///< [15:14] Post Sync Operation action
        u32 generic_media_state_clear : 1;          ///< [16] Invalidate generic media state context
        u32 reserved1_17 : 1;                       ///< [17] MBZ
        u32 tlb_invalidate : 1;                     ///< [18] Invalidate Render Engine TLBs
        u32 global_snapshot_count_reset : 1;        ///< [19] Reset snapshot counts / Statistics
        u32 command_streamer_stall_enable : 1;      ///< [20] CS Stall (act like legacy MI_FLUSH)
        u32 store_data_index : 1;                   ///< [21] Address is index into Per-Process HW Status Page
        u32 reserved1_22 : 10;                      ///< [31:22] Reserved / Optional LRI bit

        // ====================================================================
        // DWord 2..3: Destination Address
        // ====================================================================
        u64 reserved2_0 : 2;              ///< [1:0]  MBZ
        u64 destination_address_type : 1; ///< [2]    0=PPGTT, 1=GGTT (Ignored if post_sync_operation=NO_WRITE)
        u64 destination_address : 61;     ///< [63:3] 8-byte aligned destination address

        // ====================================================================
        // DWord 4..5: Immediate Data
        // ====================================================================
        u64 immediate_data;               ///< [63:0] Data to write if post_sync_operation=WRITE_IMMEDIATE_DATA
    } __attribute__((packed));

    u32 raw[6];

    /**
     * @brief Creates a default-initialized PIPE_CONTROL command.
     */
    [[nodiscard]] static constexpr PIPE_CONTROL create() {
        PIPE_CONTROL cmd{};
        cmd.dword_length = 0x4; // 6 DWords total - 2 = 4
        cmd.sub_opcode   = 0x00; // PIPE_CONTROL
        cmd.opcode       = 0x2;  // 3D_CONTROL
        cmd.sub_type     = 0x3;  // GFXPIPE_3D
        cmd.type         = 0x3;  // GFXPIPE
        return cmd;
    }

    /**
     * @brief Sets up a memory write post-sync operation.
     *
     * @param addr      The 8-byte aligned memory address.
     * @param data      The 64-bit value to write.
     * @param use_ggtt  true for GGTT address space, false for PPGTT.
     */
    constexpr void set_write_immediate(u64 addr, u64 data, bool use_ggtt = true) {
        post_sync_operation = WRITE_IMMEDIATE_DATA;
        destination_address_type = use_ggtt ? 1 : 0;
        destination_address = (addr >> 3);
        immediate_data = data;
    }

    /**
     * @brief Sets up a timestamp write post-sync operation.
     *
     * @param addr      The 8-byte aligned memory address.
     * @param use_ggtt  true for GGTT address space, false for PPGTT.
     */
    constexpr void set_write_timestamp(u64 addr, bool use_ggtt = true) {
        post_sync_operation = WRITE_TIMESTAMP;
        destination_address_type = use_ggtt ? 1 : 0;
        destination_address = (addr >> 3);
    }
};

static_assert(sizeof(PIPE_CONTROL) == 24, "PIPE_CONTROL must be exactly 24 bytes (6 DWords)");

#endif  // VESPERAOS_CMD_PIPE_CONTROL_H