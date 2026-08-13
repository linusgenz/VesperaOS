// 3dstate_constant_body.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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

#ifndef VESPERAOS_CMD_3DSTATE_CONSTANT_BODY_H
#define VESPERAOS_CMD_3DSTATE_CONSTANT_BODY_H

#include <vespera/types.h>

/**
 * @brief 3DSTATE_CONSTANT(Body) structure (10 DWords / 320 bits).
 *
 * Represents the shared body payload embedded in 3DSTATE_CONSTANT_* commands
 * (VS, HS, DS, GS, PS). Defines read lengths and 64-bit pointers for up to 4
 * constant buffers (Buffer 0 to 3).
 *
 * @note Read lengths are in 256-bit (32-byte) units.
 * @note The sum of all four read length fields must be <= 64.
 * @note Constant buffers must be allocated in linear (untiled) graphics memory.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 4-5 (3DSTATE_CONSTANT(Body))
 */
struct THREE_DSTATE_CONSTANT_BODY {
    // ====================================================================
    // DWord 0 - Constant Buffer 0 & 1 Read Lengths
    // ====================================================================
    u16 buffer0_read_length; ///< [15:0]  Buffer 0 Read Length in 256-bit (32-byte) units
    u16 buffer1_read_length; ///< [31:16] Buffer 1 Read Length in 256-bit (32-byte) units

    // ====================================================================
    // DWord 1 - Constant Buffer 2 & 3 Read Lengths
    // ====================================================================
    u16 buffer2_read_length; ///< [15:0]  Buffer 2 Read Length in 256-bit (32-byte) units
    u16 buffer3_read_length; ///< [31:16] Buffer 3 Read Length in 256-bit (32-byte) units

    // ====================================================================
    // DWord 2..3 - Constant Buffer 0 Pointer (GraphicsAddress[63:5])
    // ====================================================================
    u32 buffer0_pointer_low;  ///< DW2  Pointer 0 [31:5], bits [4:0] MBZ
    u32 buffer0_pointer_high; ///< DW3  Pointer 0 [63:32], bits [31:16] MBZ (canonical [63:48]==[47])

    // ====================================================================
    // DWord 4..5 - Constant Buffer 1 Pointer (GraphicsAddress[63:5])
    // ====================================================================
    u32 buffer1_pointer_low;  ///< DW4  Pointer 1 [31:5], bits [4:0] MBZ
    u32 buffer1_pointer_high; ///< DW5  Pointer 1 [63:32], bits [31:16] MBZ (canonical [63:48]==[47])

    // ====================================================================
    // DWord 6..7 - Constant Buffer 2 Pointer (GraphicsAddress[63:5])
    // ====================================================================
    u32 buffer2_pointer_low;  ///< DW6  Pointer 2 [31:5], bits [4:0] MBZ
    u32 buffer2_pointer_high; ///< DW7  Pointer 2 [63:32], bits [31:16] MBZ (canonical [63:48]==[47])

    // ====================================================================
    // DWord 8..9 - Constant Buffer 3 Pointer (GraphicsAddress[63:5])
    // ====================================================================
    u32 buffer3_pointer_low;  ///< DW8  Pointer 3 [31:5], bits [4:0] MBZ
    u32 buffer3_pointer_high; ///< DW9  Pointer 3 [63:32], bits [31:16] MBZ (canonical [63:48]==[47])

    /**
     * @brief Creates a constant body configured for a single constant buffer (Buffer 0).
     *
     * @param buffer0_address 64-bit virtual address/offset of Buffer 0 (32-byte aligned).
     * @param read_length_32b Length in 32-byte (256-bit) units (0-64).
     */
    [[nodiscard]] static constexpr THREE_DSTATE_CONSTANT_BODY create_buffer0(
        u64 buffer0_address,
        u16 read_length_32b
    ) {
        THREE_DSTATE_CONSTANT_BODY body{};
        if (read_length_32b > 0) {
            body.buffer0_read_length = read_length_32b;
            body.buffer0_pointer_low = static_cast<u32>(buffer0_address & 0xFFFFFFFF);
            body.buffer0_pointer_high = static_cast<u32>(buffer0_address >> 32);
        }
        return body;
    }

    /**
     * @brief Creates a constant body configured for Buffer 1.
     */
    [[nodiscard]] static constexpr THREE_DSTATE_CONSTANT_BODY create_buffer1(
        u64 buffer1_address,
        u16 read_length_32b
    ) {
        THREE_DSTATE_CONSTANT_BODY body{};
        if (read_length_32b > 0) {
            body.buffer1_read_length = read_length_32b;
            body.buffer1_pointer_low = static_cast<u32>(buffer1_address & 0xFFFFFFFF);
            body.buffer1_pointer_high = static_cast<u32>(buffer1_address >> 32);
        }
        return body;
    }

    /**
     * @brief Creates a constant body configured for Buffer 2.
     */
    [[nodiscard]] static constexpr THREE_DSTATE_CONSTANT_BODY create_buffer2(
        u64 buffer2_address,
        u16 read_length_32b
    ) {
        THREE_DSTATE_CONSTANT_BODY body{};
        if (read_length_32b > 0) {
            body.buffer2_read_length = read_length_32b;
            body.buffer2_pointer_low = static_cast<u32>(buffer2_address & 0xFFFFFFFF);
            body.buffer2_pointer_high = static_cast<u32>(buffer2_address >> 32);
        }
        return body;
    }

    /**
     * @brief Creates a constant body configured for Buffer 3.
     */
    [[nodiscard]] static constexpr THREE_DSTATE_CONSTANT_BODY create_buffer3(
        u64 buffer3_address,
        u16 read_length_32b
    ) {
        THREE_DSTATE_CONSTANT_BODY body{};
        if (read_length_32b > 0) {
            body.buffer3_read_length = read_length_32b;
            body.buffer3_pointer_low = static_cast<u32>(buffer3_address & 0xFFFFFFFF);
            body.buffer3_pointer_high = static_cast<u32>(buffer3_address >> 32);
        }
        return body;
    }
} __attribute__((packed));

static_assert(sizeof(THREE_DSTATE_CONSTANT_BODY) == 40,
              "THREE_DSTATE_CONSTANT_BODY must be exactly 10 DWords (40 bytes)");

#endif  // VESPERAOS_CMD_3DSTATE_CONSTANT_BODY_H
