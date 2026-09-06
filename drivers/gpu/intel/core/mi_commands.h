// mi_commands.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.08.26.
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

#ifndef VESPERAOS_MI_COMMANDS_H
#define VESPERAOS_MI_COMMANDS_H

#include <vespera/types.h>

namespace gpu::intel::core {
    /// Command client IDs für Command-Header.
    enum CommandClient : u32 {
        CMD_MI           = 0x0, // Memory Interface (MI_* commands)
        CMD_PROCESSOR_2D = 0x2, // 2D Blitter Engine (XY_* commands)
        CMD_RENDER_3D    = 0x3, // 3D/Render Engine (3DSTATE_* commands)
    };

    /// MI Command Opcodes (Bits [28:23])
    enum MiOpcode : u32 {
        OPCODE_MI_NOOP           = 0x00,
        OPCODE_MI_USER_INTERRUPT = 0x02,
        OPCODE_MI_WAIT_FOR_EVENT = 0x03,
    };

    constexpr u32 MI_NOOP = 0x00000000;

    // --- MI_USER_INTERRUPT ---
    union MI_USER_INTERRUPT_CMD {
        struct {
            u32 reserved : 23; // [22:0] MBZ
            u32 opcode   : 6;  // [28:23] MI Command Opcode = 0x02
            u32 client   : 3;  // [31:29] Command Type = MI_COMMAND (0)
        } __attribute__((packed));

        u32 raw;
    };

    // --- MI_WAIT_FOR_EVENT ---
    union MI_WAIT_FOR_EVENT_CMD {
        struct {
            u32 display_plane_1_a_scan_line_wait      : 1; // [0]
            u32 display_plane_1_a_flip_pending_wait   : 1; // [1]
            u32 display_plane_2_a_flip_pending_wait   : 1; // [2]
            u32 display_plane_1_a_vertical_blank_wait : 1; // [3]
            u32 reserved_5_4                          : 2; // [5:4]   MBZ
            u32 display_plane_3_a_flip_pending_wait   : 1; // [6]
            u32 display_plane_3_b_flip_pending_wait   : 1; // [7]
            u32 display_plane_1_b_scan_line_wait      : 1; // [8]
            u32 display_plane_1_b_flip_pending_wait   : 1; // [9]
            u32 display_plane_2_b_flip_pending_wait   : 1; // [10]
            u32 display_plane_1_b_vertical_blank_wait : 1; // [11]
            u32 reserved_13_12                        : 2; // [13:12] MBZ
            u32 display_plane_1_c_scan_line_wait      : 1; // [14]
            u32 display_plane_1_c_flip_pending_wait   : 1; // [15]
            u32 display_plane_3_c_flip_pending_wait   : 1; // [16]
            u32 display_plane_4_a_flip_pending_wait   : 1; // [17]
            u32 display_plane_4_b_flip_pending_wait   : 1; // [18]
            u32 display_plane_4_c_flip_pending_wait   : 1; // [19]
            u32 display_plane_2_c_flip_pending_wait   : 1; // [20]
            u32 display_plane_1_c_vertical_blank_wait : 1; // [21]
            u32 reserved_22                           : 1; // [22]    MBZ
            u32 opcode                                : 6; // [28:23] = 0x03
            u32 client                                : 3; // [31:29] = 0x0 (MI_COMMAND)
        } __attribute__((packed));

        u32 raw;
    };

    // --- MI_FLUSH_DW ---
    union MI_FLUSH_DW_DW0 {
        struct {
            u32 dword_len   : 6; // [5:0]
            u32 reserved_a  : 2; // [7:6]   MBZ
            u32 notify_en   : 1; // [8]
            u32 flush_llc   : 1; // [9]
            u32 reserved_b  : 4; // [13:10] MBZ
            u32 post_sync   : 2; // [15:14] 2-Bit
            u32 reserved_c  : 2; // [17:16]
            u32 tlb_inv     : 1; // [18]
            u32 reserved_d  : 2; // [20:19] MBZ
            u32 store_index : 1; // [21]
            u32 reserved_e  : 1; // [22]
            u32 opcode      : 6; // [28:23]
            u32 client      : 3; // [31:29]
        } __attribute__((packed));

        u32 raw;
    };

    union MI_FLUSH_DW_DW1 {
        struct {
            u32 reserved_a : 2;  // [1:0]   MBZ
            u32 addr_lo    : 30; // [31:2]
        } __attribute__((packed));

        u32 raw;
    };

    union MI_FLUSH_DW_DW2 {
        struct {
            u32 addr_hi    : 16; // [15:0]
            u32 reserved_a : 16; // [31:16] MBZ
        } __attribute__((packed));

        u32 raw;
    };

    struct MI_FLUSH_DW_CMD {
        MI_FLUSH_DW_DW0 dw0;
        MI_FLUSH_DW_DW1 dw1;
        MI_FLUSH_DW_DW2 dw2;
        u64 immediate_data;
    } __attribute__((packed));

    /**
 * @brief Address Space Indicator for MI_BATCH_BUFFER_START.
 */
    enum AddressSpaceIndicator : u32 {
        ADDRESS_SPACE_GGTT  = 0x0, ///< Located in GGTT memory (Privileged)
        ADDRESS_SPACE_PPGTT = 0x1, ///< Located in PPGTT memory (Non-Privileged)
    };

    /**
     * @brief Second Level Batch Buffer indicator.
     */
    enum SecondLevelBatchBuffer : u32 {
        BATCH_LEVEL_FIRST  = 0x0, ///< Place batch address in 1st level batch storage
        BATCH_LEVEL_SECOND = 0x1, ///< Place batch address in 2nd level batch storage
    };

    /**
     * @brief MI_BATCH_BUFFER_START command structure (2 DWords / 64 bits).
     *
     * Initiates execution of commands stored in a batch buffer. Can be used for
     * first-level chaining or second-level batch buffer invocation.
     *
     * @note Graphics Address is a 48-bit GPU virtual address [47:0] where bits [1:0] MBZ.
     * @note DWord length bias is 2, so dword_length = 0x1 (2 DWords - 2).
     *
     * @see IHD-OS-KBL-Vol 2a-1.17, pp. 968-971 (MI_BATCH_BUFFER_START)
     */
    union MI_BATCH_BUFFER_START {
        enum CommandOpcode : u32 {
            OPCODE_MI_BATCH_BUFFER_START = 0x31,
        };

        struct {
            // ====================================================================
            // DWord 0
            // ====================================================================
            u32 dword_length              : 8; ///< [7:0]   Default: 0x1 (2 DWords - 2)
            u32 address_space_indicator   : 1; ///< [8]     AddressSpaceIndicator (0 = GGTT, 1 = PPGTT)
            u32 reserved0_9               : 1; ///< [9]     MBZ
            u32 resource_streamer_enable  : 1; ///< [10]    RenderCS specific: Resource Streamer enable
            u32 reserved0_11              : 4; ///< [14:11] MBZ
            u32 predication_enable        : 1; ///< [15]    RenderCS specific: Enable predication
            u32 add_offset_enable         : 1; ///< [16]    RenderCS specific: Add BB OFFSET MMIO register
            u32 reserved0_17              : 2; ///< [18:17] MBZ
            u32 reserved0_19              : 1; ///< [19]    MBZ
            u32 reserved0_20              : 2; ///< [21:20] MBZ
            u32 second_level_batch_buffer : 1; ///< [22]    SecondLevelBatchBuffer (0 = First level, 1 = Second level)
            u32 opcode                    : 6; ///< [28:23] Default: 0x31 (MI_BATCH_BUFFER_START)
            u32 command_type              : 3; ///< [31:29] Default: 0x0 (MI_COMMAND)

            // ====================================================================
            // DWord 1
            // ====================================================================
            u32 batch_buffer_start_address_low : 32;
            ///< [31:0] Lower 32 bits of GraphicsAddress [31:2] (bits [1:0] MBZ)
        } __attribute__((packed));

        u32 raw[2];

        /**
         * @brief Creates an MI_BATCH_BUFFER_START command for a 32-bit GPU address.
         *
         * @param gpu_address             32-bit or 48-bit GPU address (DWord-aligned, bits [1:0] == 0).
         * @param is_ppgtt                true for PPGTT (Non-Privileged), false for GGTT (Privileged).
         * @param is_second_level         true for 2nd level batch buffer, false for 1st level chain.
         * @param add_offset              RenderCS: Add BB_OFFSET MMIO register to start address.
         * @param predication             RenderCS: Enable predication based on MI_PREDICATE_RESULT_1.
         */
        [[nodiscard]] static constexpr MI_BATCH_BUFFER_START create(
            u64 gpu_address,
            bool is_ppgtt = true,
            bool is_second_level = false,
            bool add_offset = false,
            bool predication = false
        ) {
            MI_BATCH_BUFFER_START cmd{};
            cmd.command_type = CMD_MI;
            cmd.opcode = OPCODE_MI_BATCH_BUFFER_START;
            cmd.second_level_batch_buffer = is_second_level ? BATCH_LEVEL_SECOND : BATCH_LEVEL_FIRST;
            cmd.add_offset_enable = add_offset ? 1 : 0;
            cmd.predication_enable = predication ? 1 : 0;
            cmd.resource_streamer_enable = 0;
            cmd.address_space_indicator = is_ppgtt ? ADDRESS_SPACE_PPGTT : ADDRESS_SPACE_GGTT;
            cmd.dword_length = 0x1; // 2 DWords total - 2 = 1

            // GraphicsAddress[31:2], bits [1:0] MBZ
            cmd.batch_buffer_start_address_low = static_cast<u32>(gpu_address & 0xFFFFFFFF) & ~0x3U;

            return cmd;
        }
    };

    static_assert(sizeof(MI_BATCH_BUFFER_START) == 8, "MI_BATCH_BUFFER_START must be exactly 2 DWords (8 bytes)");


    /**
     * @brief MI_BATCH_BUFFER_END command structure (1 DWord / 32 bits).
     *
     * Terminates the execution of commands stored in a batch buffer initiated
     * using a MI_BATCH_BUFFER_START command.
     *
     * @see IHD-OS-KBL-Vol 2a-1.17 (MI_BATCH_BUFFER_END)
     */
    union MI_BATCH_BUFFER_END {
        enum CommandOpcode : u32 {
            OPCODE_MI_BATCH_BUFFER_END = 0x0A,
        };

        struct {
            u32 reserved0_0  : 23; ///< [22:0]  MBZ
            u32 opcode       : 6;  ///< [28:23] Default: 0x0A (MI_BATCH_BUFFER_END)
            u32 command_type : 3;  ///< [31:29] Default: 0x0  (MI_COMMAND)
        } __attribute__((packed));

        u32 raw;

        /**
         * @brief Creates a default-initialized MI_BATCH_BUFFER_END command.
         */
        [[nodiscard]] static constexpr MI_BATCH_BUFFER_END create() {
            MI_BATCH_BUFFER_END cmd{};
            cmd.command_type = CMD_MI;
            cmd.opcode = OPCODE_MI_BATCH_BUFFER_END;
            cmd.reserved0_0 = 0;
            return cmd;
        }
    };

    static_assert(sizeof(MI_BATCH_BUFFER_END) == 4, "MI_BATCH_BUFFER_END must be exactly 1 DWord (4 bytes)");

    /**
     * @brief MI_STORE_DATA_IMM command (5 DWords minimum for QWord write).
     *
     * Requests a write of the immediate data supplied in the packet to the specified Memory Address.
     * Writes target System Memory Address and are coherent with CPU cache.
     *
     * @see IHD-OS-KBL-Vol 2a-1.17, pp. 1049-1050 (MI_STORE_DATA_IMM)
     */
    union MI_STORE_DATA_IMM {
        enum CommandOpcode : u32 {
            OPCODE_MI_STORE_DATA_IMM = 0x20,
        };

        enum DWordLength : u32 {
            LENGTH_STORE_DWORD = 0x2,
            LENGTH_STORE_QWORD = 0x3,
        };

        struct {
            // DWord 0
            u32 dword_length      : 10; ///< [9:0]   Total Length - 2. Default: 0x2 (DWord) or 0x3 (QWord)
            u32 reserved0_10      : 11; ///< [20:10] MBZ
            u32 store_qword       : 1;  ///< [21]    1 = Store QWord, 0 = Store DWord
            u32 use_global_gtt    : 1;  ///< [22]    1 = Use Global GTT, 0 = Use PPGTT
            u32 mi_command_opcode : 6;  ///< [28:23] Default: 0x20 (MI_STORE_DATA_IMM)
            u32 command_type      : 3;  ///< [31:29] Default: 0x0 (MI_COMMAND)

            // DWord 1
            u32 core_mode_enable : 1;  ///< [0]     1 = Offset address by Core ID
            u32 reserved1_1      : 1;  ///< [1]     MBZ
            u32 address          : 30; ///< [31:2]  GraphicsAddress[31:2] (Aligned)

            // DWord 2
            u32 address_high : 16; ///< [15:0]  GraphicsAddress[47:32]
            u32 reserved2_16 : 16; ///< [31:16] MBZ

            // DWord 3
            u32 data_dword_0; ///< [31:0]  Lower DWord of data / DWord value

            // DWord 4
            u32 data_dword_1; ///< [31:0]  Upper DWord of data (for QWord store)
        } __attribute__((packed));

        u32 raw[5];

        /**
         * @brief Creates a default-initialized MI_STORE_DATA_IMM command for QWord stores.
         */
        [[nodiscard]] static constexpr MI_STORE_DATA_IMM create() {
            MI_STORE_DATA_IMM cmd{};
            cmd.dword_length = LENGTH_STORE_QWORD;
            cmd.store_qword = 1;
            cmd.use_global_gtt = 0;
            cmd.mi_command_opcode = OPCODE_MI_STORE_DATA_IMM;
            cmd.command_type = CMD_MI;
            return cmd;
        }

        /**
         * @brief Creates an MI_STORE_DATA_IMM command for a 64-bit QWord store operation.
         *
         * @param graphics_address The 64-bit target graphics address (Must be 8B aligned).
         * @param data The 64-bit QWord value to store.
         * @param use_ggtt Set to true to use Global GTT, false for PPGTT.
         */
        [[nodiscard]] static constexpr MI_STORE_DATA_IMM create_qword(
            u64 graphics_address, u64 data, bool use_ggtt = false
        ) {
            MI_STORE_DATA_IMM cmd = create();
            cmd.store_qword = 1;
            cmd.dword_length = LENGTH_STORE_QWORD;
            cmd.use_global_gtt = use_ggtt ? 1 : 0;

            // Address assignment (Bits [47:2] valid)
            cmd.address = static_cast<u32>((graphics_address >> 2) & 0x3FFFFFFF);
            cmd.address_high = static_cast<u32>((graphics_address >> 32) & 0xFFFF);

            // Data payload
            cmd.data_dword_0 = static_cast<u32>(data & 0xFFFFFFFF);
            cmd.data_dword_1 = static_cast<u32>((data >> 32) & 0xFFFFFFFF);

            return cmd;
        }

        /**
         * @brief Creates an MI_STORE_DATA_IMM command for a 32-bit DWord store operation.
         *
         * @param graphics_address The 64-bit target graphics address (Must be 4B aligned).
         * @param data The 32-bit DWord value to store.
         * @param use_ggtt Set to true to use Global GTT, false for PPGTT.
         */
        [[nodiscard]] static constexpr MI_STORE_DATA_IMM create_dword(
            u64 graphics_address, u32 data, bool use_ggtt = false
        ) {
            MI_STORE_DATA_IMM cmd = create();
            cmd.store_qword = 0;
            cmd.dword_length = LENGTH_STORE_DWORD;
            cmd.use_global_gtt = use_ggtt ? 1 : 0;

            cmd.address = static_cast<u32>((graphics_address >> 2) & 0x3FFFFFFF);
            cmd.address_high = static_cast<u32>((graphics_address >> 32) & 0xFFFF);

            cmd.data_dword_0 = data;
            cmd.data_dword_1 = 0;

            return cmd;
        }
    };

    static_assert(sizeof(MI_STORE_DATA_IMM) == 20, "MI_STORE_DATA_IMM must be 5 DWords (20 bytes)");

    /**
     * @brief MI_LOAD_REGISTER_IMM command (3 DWords for a single register write).
     *
     * Requests a write of up to a DWord constant supplied in the command
     * to the specified Register Offset (MMIO range).
     *
     * @see IHD-OS-KBL-Vol 2a-1.17, pp. 1002 (MI_LOAD_REGISTER_IMM)
     */
    union MI_LOAD_REGISTER_IMM {
        enum CommandOpcode : u32 {
            OPCODE_MI_LOAD_REGISTER_IMM = 0x22,
        };

        enum CommandType : u32 {
            CMD_MI = 0x0,
        };

        struct {
            // DWord 0
            u32 dword_length         : 8;  ///< [7:0]   Total Length - 2. Default: 0x1 (1 DWord extra)
            u32 byte_write_disables  : 4;  ///< [11:8]  Bit 8=Byte0, Bit 11=Byte3. '1111b' = NOOP
            u32 reserved0_12         : 1;  ///< [12]    Reserved
            u32 reserved0_13         : 6;  ///< [18:13] MBZ
            u32 reserved0_19         : 1;  ///< [19]    Reserved
            u32 reserved0_20         : 3;  ///< [22:20] MBZ
            u32 mi_command_opcode    : 6;  ///< [28:23] Default: 0x22 (MI_LOAD_REGISTER_IMM)
            u32 command_type         : 3;  ///< [31:29] Default: 0x0 (MI_COMMAND)

            // DWord 1
            u32 reserved1_0          : 2;  ///< [1:0]   MBZ
            u32 register_offset      : 21; ///< [22:2]  Bits [22:2] of Register Offset (MMIO DWord Offset)
            u32 reserved1_23         : 9;  ///< [31:23] MBZ

            // DWord 2
            u32 data_dword;                ///< [31:0]  32-bit Data DWord to write to register
        } __attribute__((packed));

        u32 raw[3];

        /**
         * @brief Creates a default-initialized MI_LOAD_REGISTER_IMM command.
         */
        [[nodiscard]] static constexpr MI_LOAD_REGISTER_IMM create() {
            MI_LOAD_REGISTER_IMM cmd{};
            cmd.dword_length = 1; // 3 DWords total -> Length - 2 = 1
            cmd.byte_write_disables = 0; // All bytes enabled by default
            cmd.mi_command_opcode = OPCODE_MI_LOAD_REGISTER_IMM;
            cmd.command_type = CMD_MI;
            return cmd;
        }

        /**
         * @brief Creates an MI_LOAD_REGISTER_IMM command for writing a 32-bit value to a MMIO register offset.
         *
         * @param mmio_offset The byte-aligned MMIO register offset (e.g. 0x20D8). Bits [1:0] will be stripped.
         * @param data The 32-bit DWord value to write to the register.
         * @param byte_disables Mask of byte write disables (4 bits, default 0 = enable all bytes).
         */
        [[nodiscard]] static constexpr MI_LOAD_REGISTER_IMM create(
            u32 mmio_offset, u32 data, u8 byte_disables = 0
        ) {
            MI_LOAD_REGISTER_IMM cmd = create();
            cmd.byte_write_disables = byte_disables & 0xF;
            cmd.register_offset = (mmio_offset >> 2) & 0x1FFFFF; // Bits [22:2]
            cmd.data_dword = data;

            return cmd;
        }
    };

    static_assert(sizeof(MI_LOAD_REGISTER_IMM) == 12, "MI_LOAD_REGISTER_IMM must be 3 DWords (12 bytes)");
} // namespace gpu::intel::core

#endif // VESPERAOS_MI_COMMANDS_H
