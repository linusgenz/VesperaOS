// cmd_state_base_address.h
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

#ifndef VESPERAOS_CMD_STATE_BASE_ADDRESS_H
#define VESPERAOS_CMD_STATE_BASE_ADDRESS_H

#include <vespera/types.h>

/**
 * @brief STATE_BASE_ADDRESS Command (3DSTATE_NONPIPELINED).
 *
 * This command sets the base pointers and sizes for various state types
 * (General, Surface, Dynamic, Indirect Object, Instruction, and Bindless).
 * It establishes the canonical memory regions the GPU will use to fetch
 * these respective indirect state objects.
 *
 * @note All base addresses must be 4K-byte aligned.
 * @note Hardware ignores address bits [63:48] and assumes standard x86_64
 *       canonical form where [63:48] == [47].
 * @note General State Base Address [47:12] + General State Buffer Size [31:12]
 *       must be strictly less than 2^48.
 * @note The Resource Streamer (RS) must be disabled before updating the
 *       Surface State Base Address.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 1214–1220 (STATE_BASE_ADDRESS)
 */
union STATE_BASE_ADDRESS {
    struct {
        // ====================================================================
        // DWord 0: Command Header
        // ====================================================================
        /**
         * @brief DWord Length [7:0]
         *
         * Excludes DWord (0,1). The default length is 11h (17) for the full
         * 19-DWord command.
         */
        u32 dword_length : 8;        ///< [7:0]   Default: 0x11
        u32 reserved0_8 : 8;         ///< [15:8]  MBZ
        u32 sub_opcode : 8;          ///< [23:16] Default: 0x01 (STATE_BASE_ADDRESS)
        u32 opcode : 3;              ///< [26:24] Default: 0x1  (GFXPIPE_NONPIPELINED)
        u32 sub_type : 2;            ///< [28:27] Default: 0x0  (3D Command)
        u32 type : 3;                ///< [31:29] Default: 0x3  (GFXPIPE)

        // ====================================================================
        // DWord 1..2: General State Base Address
        // ====================================================================
        u64 general_state_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved1_1 : 3;                 ///< [3:1]   MBZ
        u64 general_state_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved1_11 : 1;                ///< [11]    MBZ
        u64 general_state_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 3: Stateless Data Port Access
        // ====================================================================
        u32 reserved3_0 : 16;                  ///< [15:0]  MBZ
        u32 stateless_data_port_mocs : 7;      ///< [22:16] Stateless Data Port Access MOCS
        u32 reserved3_23 : 9;                  ///< [31:23] MBZ

        // ====================================================================
        // DWord 4..5: Surface State Base Address
        // ====================================================================
        u64 surface_state_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved4_1 : 3;                 ///< [3:1]   MBZ
        u64 surface_state_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved4_11 : 1;                ///< [11]    MBZ
        u64 surface_state_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 6..7: Dynamic State Base Address
        // ====================================================================
        u64 dynamic_state_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved6_1 : 3;                 ///< [3:1]   MBZ
        u64 dynamic_state_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved6_11 : 1;                ///< [11]    MBZ
        u64 dynamic_state_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 8..9: Indirect Object Base Address
        // ====================================================================
        u64 indirect_object_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved8_1 : 3;                   ///< [3:1]   MBZ
        u64 indirect_object_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved8_11 : 1;                  ///< [11]    MBZ
        u64 indirect_object_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 10..11: Instruction Base Address
        // ====================================================================
        u64 instruction_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved10_1 : 3;              ///< [3:1]   MBZ
        u64 instruction_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved10_11 : 1;             ///< [11]    MBZ
        u64 instruction_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 12: General State Buffer Size
        // ====================================================================
        u32 general_state_size_modify_enable : 1; ///< [0]     Modify Enable
        u32 reserved12_1 : 11;                    ///< [11:1]  MBZ
        u32 general_state_buffer_size : 20;       ///< [31:12] Size in 4K pages

        // ====================================================================
        // DWord 13: Dynamic State Buffer Size
        // ====================================================================
        u32 dynamic_state_size_modify_enable : 1; ///< [0]     Modify Enable
        u32 reserved13_1 : 11;                    ///< [11:1]  MBZ
        u32 dynamic_state_buffer_size : 20;       ///< [31:12] Size in 4K pages

        // ====================================================================
        // DWord 14: Indirect Object Buffer Size
        // ====================================================================
        u32 indirect_object_size_modify_enable : 1; ///< [0]     Modify Enable
        u32 reserved14_1 : 11;                      ///< [11:1]  MBZ
        u32 indirect_object_buffer_size : 20;       ///< [31:12] Size in 4K pages

        // ====================================================================
        // DWord 15: Instruction Buffer Size
        // ====================================================================
        u32 instruction_size_modify_enable : 1; ///< [0]     Modify Enable
        u32 reserved15_1 : 11;                  ///< [11:1]  MBZ
        u32 instruction_buffer_size : 20;       ///< [31:12] Size in 4K pages

        // ====================================================================
        // DWord 16..17: Bindless Surface State Base Address
        // ====================================================================
        u64 bindless_surface_modify_enable : 1; ///< [0]     Modify Enable
        u64 reserved16_1 : 3;                   ///< [3:1]   MBZ
        u64 bindless_surface_mocs : 7;          ///< [10:4]  Memory Object Control State
        u64 reserved16_11 : 1;                  ///< [11]    MBZ
        u64 bindless_surface_base_address : 52; ///< [63:12] 4K-aligned Base Address

        // ====================================================================
        // DWord 18: Bindless Surface State Size
        // ====================================================================
        u32 reserved18_0 : 12;               ///< [11:0]  MBZ
        u32 bindless_surface_state_size : 20;///< [31:12] Size-1 in 64-Byte increments
    } __attribute__((packed));

    u32 raw[19];

    /**
     * @brief Creates a default-initialized STATE_BASE_ADDRESS command.
     *
     * All modify enable bits are 0 by default. Software must explicitly call
     * the respective `set_*` functions to arm the modify bits.
     */
    [[nodiscard]] static constexpr STATE_BASE_ADDRESS create() {
        STATE_BASE_ADDRESS cmd{};
        cmd.dword_length = 0x11; // 19 DWords total - 2 = 17 (0x11)
        cmd.sub_opcode   = 0x01; // STATE_BASE_ADDRESS
        cmd.opcode       = 0x1;  // 3DSTATE_NONPIPELINED
        cmd.sub_type     = 0x0;  // 3D
        cmd.type         = 0x3;  // GFXPIPE
        return cmd;
    }

    /**
     * @brief Sets the General State base address and size.
     *
     * @param addr          4K-aligned GTT base address.
     * @param size_4k_pages Size of the buffer in 4K pages (0 = no valid data).
     * @param mocs          Memory Object Control State.
     */
    constexpr void set_general_state(u64 addr, u32 size_4k_pages, u8 mocs = 0) {
        general_state_base_address = (addr >> 12);
        general_state_mocs = mocs;
        general_state_modify_enable = 1;

        general_state_buffer_size = size_4k_pages;
        general_state_size_modify_enable = 1;
    }

    /**
     * @brief Sets the Surface State base address.
     *
     * @param addr 4K-aligned GTT base address.
     * @param mocs Memory Object Control State.
     */
    constexpr void set_surface_state(u64 addr, u8 mocs = 0) {
        surface_state_base_address = (addr >> 12);
        surface_state_mocs = mocs;
        surface_state_modify_enable = 1;
    }

    /**
     * @brief Sets the Dynamic State base address and size.
     *
     * @param addr          4K-aligned GTT base address.
     * @param size_4k_pages Size of the buffer in 4K pages (0 = no valid data).
     * @param mocs          Memory Object Control State.
     */
    constexpr void set_dynamic_state(u64 addr, u32 size_4k_pages, u8 mocs = 0) {
        dynamic_state_base_address = (addr >> 12);
        dynamic_state_mocs = mocs;
        dynamic_state_modify_enable = 1;

        dynamic_state_buffer_size = size_4k_pages;
        dynamic_state_size_modify_enable = 1;
    }

    /**
     * @brief Sets the Instruction State base address and size.
     *
     * @param addr          4K-aligned GTT base address.
     * @param size_4k_pages Size of the buffer in 4K pages (0 = no valid data).
     * @param mocs          Memory Object Control State.
     */
    constexpr void set_instruction_state(u64 addr, u32 size_4k_pages, u8 mocs = 0) {
        instruction_base_address = (addr >> 12);
        instruction_mocs = mocs;
        instruction_modify_enable = 1;

        instruction_buffer_size = size_4k_pages;
        instruction_size_modify_enable = 1;
    }

    /**
     * @brief Sets the Bindless Surface State base address and size.
     *
     * @param addr               4K-aligned GTT base address.
     * @param num_surface_states The number of maximum bindless surface states.
     *                           (1 increment = 64 bytes).
     * @param mocs               Memory Object Control State.
     */
    constexpr void set_bindless_surface_state(u64 addr, u32 num_surface_states, u8 mocs = 0) {
        bindless_surface_base_address = (addr >> 12);
        bindless_surface_mocs = mocs;
        bindless_surface_modify_enable = 1;

        if (num_surface_states > 0) {
            bindless_surface_state_size = num_surface_states - 1;
        } else {
            bindless_surface_state_size = 0;
        }
    }
};

static_assert(sizeof(STATE_BASE_ADDRESS) == 76, "STATE_BASE_ADDRESS must be exactly 76 bytes (19 DWords)");

#endif //VESPERAWORKSPACE_CMD_STATE_BASE_ADDRESS_H
