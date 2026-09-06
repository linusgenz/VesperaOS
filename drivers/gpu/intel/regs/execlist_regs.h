// execlist_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.08.26.
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

#ifndef VESPERAOS_EXECLIST_REGS_H
#define VESPERAOS_EXECLIST_REGS_H

#include <vespera/types.h>

/**
 * CONTEXT_DESCRIPTOR - Context Descriptor Format
 *
 * Not a memory-mapped register - this is the 64-bit value software assembles in memory/locals and
 * writes DWord-by-DWord (low DWord first) to EXECLIST_SUBMITPORT. Two of these make up one execlist.
 *
 * @note LRCA must always be programmed as a GGTT graphics address
 */
union CONTEXT_DESCRIPTOR {
    struct {
        u32 valid            : 1;  ///< [0]     Valid - set for elements actually in use
        u32 reserved1        : 1;  ///< [1]     Reserved
        u32 force_restore    : 1;  ///< [2]     Force Restore - force context restore despite LRCA match
        u32 addressing_mode  : 2;  ///< [4:3]   Addressing Mode & Legacy Context (see AddressingMode enum)
        u32 reserved5        : 1;  ///< [5]     Reserved
        u32 fault_handling   : 2;  ///< [7:6]   Fault Handling - must be FAULT_AND_HANG in legacy ctx mode
        u32 privilege_access : 1;  ///< [8]     Privilege Access - PPGTT enabled in legacy context mode
        u32 reserved9_10     : 2;  ///< [10:9]  Reserved
        u32 reserved11       : 1;  ///< [11]    Reserved
        u32 lrca             : 20; ///< [31:12] Logical Ring Context Address, GraphicsAddress[31:12], GGTT-relative

        // Context ID (upper DWord, bits [63:32] of the descriptor / bits [31:0] of Context ID).
        // Per-diagram grouping (Bit 63 down to 32): Eng. ID | SW Counter | HW Use | SW Context ID.
        // The text table further splits "SW Context ID" (Context ID bits 21:0) into Engine
        // Instance, SW Context ID proper, and Virtual Function Number - modeled below at that
        // finer granularity since that's what software actually needs to set independently.
        u32 virtual_function_number : 5;  ///< [36:32] VF Number[4:0] - programming note: must always be 0x0
        u32 sw_context_id           : 11; ///< [47:37] SW Context ID - software-assigned, 2048 contexts per VF
        u32 engine_instance         : 6;  ///< [53:48] Engine Instance within Engine Class
        u32 hw_use                  : 1;  ///< [54]    HW Use - MBZ for SW, HW-managed F&H vs F&S bookkeeping
        u32 sw_counter              : 6;  ///< [60:55] SW Counter
        u32 engine_class            : 3;  ///< [63:61] Engine Class
    } __attribute__((packed));

    u64 raw;

    enum AddressingMode : u32 {
        ADVANCED_NO_AD_SUPPORT = 0b00,
        LEGACY_32BIT_PPGTT = 0b01, ///< PDP*_DESCRIPTOR hold base addresses for up to 4GB, no 64-bit VA
        ADVANCED_AD_SUPPORT = 0b10,
        LEGACY_64BIT_PPGTT = 0b11, ///< PDP0_DESCRIPTOR holds PML4 base, other PDPs ignored, 48-bit canonical VA
    };

    enum FaultHandling : u32 {
        FAULT_AND_HANG = 0b00, ///< Only supported mode - fault is treated as catastrophic
    };

    [[nodiscard]] constexpr u64 lrca_address_bytes() const {
        return static_cast<u64>(lrca) << 12;
    }

    constexpr void set_lrca_address_bytes(u64 bytes) {
        lrca = static_cast<u32>(bytes >> 12);
    }
};

static_assert(sizeof(CONTEXT_DESCRIPTOR)== 8);


// -----------------------------------------------------------------------------------------------
// EXECLIST_SUBMITPORT - Execlist Submit Port Register
//
// WO. SW writes 4 DWords per execlist submission, in this exact order:
//   Element 1 High, Element 1 Low, Element 0 High, Element 0 Low
// Writing the final DWord (Element 0 Low) triggers the submission. Unused elements must still be
// written, with their Valid bit clear.
// -----------------------------------------------------------------------------------------------
union EXECLIST_SUBMITPORT {
    struct {
        u32 context_descriptor_dw : 32; ///< [31:0] One DWord of a CONTEXT_DESCRIPTOR, per submission order above
    } __attribute__((packed));

    u32 raw;
};

static_assert(sizeof(EXECLIST_SUBMITPORT)== 4);


// -----------------------------------------------------------------------------------------------
// EXECLIST_STATUS - Execlist Status Register
//
// RO, 64-bit. Pointers/full indicator for the Execlist Queue and the context ID of the currently
// running context.
// -----------------------------------------------------------------------------------------------
union EXECLIST_STATUS {
    struct {
        u32 current_execlist_pointer : 1;  ///< [0]     Current Execlist Pointer (ExeclistContentsIndex), default 1
        u32 execlist_write_pointer   : 1;  ///< [1]     Execlist Write Pointer (ExeclistContentsIndex)
        u32 execlist_queue_full      : 1;  ///< [2]     Execlist Queue Full (only meaningful if ptrs are equal)
        u32 execlist1_valid          : 1;  ///< [3]     Execlist 1 Valid
        u32 execlist0_valid          : 1;  ///< [4]     Execlist 0 Valid
        u32 last_ctx_switch_reason   : 9;  ///< [13:5]  Last Context Switch Reason - HW use, do not write
        u32 current_active_element   : 2;  ///< [15:14] Current Active Element Status
        u32 arbitration_enable       : 1;  ///< [19:19] Arbitration Enable, mirrors MI_ARB_ON_OFF
        u32 reserved17_31            : 15; ///< [31:17] Reserved / MBZ

        u32 current_context_id : 32; ///< [63:32] Context ID of the currently running context
    } __attribute__((packed));

    u64 raw;

    enum ActiveElement : u32 {
        NO_ACTIVE_ELEMENT  = 0b00,
        ELEMENT0_EXECUTING = 0b01,
        ELEMENT1_EXECUTING = 0b10,
    };
};

static_assert(sizeof(EXECLIST_STATUS)== 8);


// -----------------------------------------------------------------------------------------------
// EXECLIST0_CONTENTS / EXECLIST1_CONTENTS - Execlist Contents
//
// RO, 128-bit mirror of what HW currently holds for the two execlist slots. Each holds two full
// context descriptors (Element 0 and Element 1). Useful for debugging only - not part of the
// submission path (see EXECLIST_SUBMITPORT for that).
// -----------------------------------------------------------------------------------------------
struct EXECLIST_CONTENTS {
    CONTEXT_DESCRIPTOR element0; ///< DWords 0-1: Element 0 (Low DWord, then High DWord)
    CONTEXT_DESCRIPTOR element1; ///< DWords 2-3: Element 1 (Low DWord, then High DWord)
};

static_assert(sizeof(EXECLIST_CONTENTS)== 16);

// Register MMIO Offsets per Engine Unit
constexpr u32 GFX_MODE_RCSUNIT   = 0x0229C;
constexpr u32 GFX_MODE_VCSUNIT0  = 0x1229C;
constexpr u32 GFX_MODE_VECSUNIT  = 0x1A29C;
constexpr u32 GFX_MODE_VCSUNIT1  = 0x1C29C;
constexpr u32 GFX_MODE_BCSUNIT   = 0x2229C;

/**
 * @brief Graphics Mode Register (GFX_MODE)
 *
 * @note Register Space: MMIO: 0/2/0
 * @note Source: BSpec (Doc Ref # IHD-OS-KBL-Vol 2c-1.17)
 * @note Default Value: 0x00000000
 * @note Access: R/W (Bits [31:16] are Write-Only Mask)
 * @note Size: 32 bits
 *
 * Controls execution mode flags such as PPGTT, Execlists, and 64-bit addressing.
 * Standard iGPU mask register semantics apply: Bit (N+16) must be set to 1
 * in order to write/update Bit (N).
 */
union GFX_MODE {
    struct {
        // --- Lower 16-Bit Values ---
        u32 privilege_check_disable    : 1;  ///< [0]     Privilege Check Disable (1 = Disable checks on non-priv BBs)
        u32 reserved1_2                : 2;  ///< [2:1]   Reserved
        u32 reserved3                  : 1;  ///< [3]     Reserved (MBZ)
        u32 reserved4                  : 1;  ///< [4]     Reserved
        u32 reserved5_6                : 2;  ///< [6:5]   Reserved
        u32 va64_bit_enable            : 1;  ///< [7]     64Bit Virtual Addressing Enable (0 = 32Bit, 1 = 64Bit/48Bit Canonical)
        u32 reserved8                  : 1;  ///< [8]     Reserved
        u32 ppgtt_enable               : 1;  ///< [9]     Per-Process GTT Enable (0 = Global GTT, 1 = PPGTT)
        u32 reserved10                 : 1;  ///< [10]    Reserved
        u32 reserved11                 : 1;  ///< [11]    Reserved
        u32 reserved12                 : 1;  ///< [12]    Reserved
        u32 reserved13                 : 1;  ///< [13]    Reserved
        u32 reserved14                 : 1;  ///< [14]    Reserved
        u32 execlist_enable            : 1;  ///< [15]    Execlist Enable (1 = Contexts submitted via Execlist)

        // --- Upper 16-Bit Write Masks ---
        u32 mask_privilege_check_disable : 1;  ///< [16]    Mask bit for privilege_check_disable
        u32 mask_reserved1_2             : 2;  ///< [18:17] Mask bits for reserved1_2
        u32 mask_reserved3               : 1;  ///< [19]    Mask bit for reserved3
        u32 mask_reserved4               : 1;  ///< [20]    Mask bit for reserved4
        u32 mask_reserved5_6             : 2;  ///< [22:21] Mask bits for reserved5_6
        u32 mask_va64_bit_enable         : 1;  ///< [23]    Mask bit for va64_bit_enable
        u32 mask_reserved8               : 1;  ///< [24]    Mask bit for reserved8
        u32 mask_ppgtt_enable            : 1;  ///< [25]    Mask bit for ppgtt_enable
        u32 mask_reserved10              : 1;  ///< [26]    Mask bit for reserved10
        u32 mask_reserved11              : 1;  ///< [27]    Mask bit for reserved11
        u32 mask_reserved12              : 1;  ///< [28]    Mask bit for reserved12
        u32 mask_reserved13              : 1;  ///< [29]    Mask bit for reserved13
        u32 mask_reserved14              : 1;  ///< [30]    Mask bit for reserved14
        u32 mask_execlist_enable         : 1;  ///< [31]    Mask bit for execlist_enable
    } __attribute__((packed));

    struct {
        u16 val;   ///< Lower 16 bits containing actual configurations
        u16 mask;  ///< Upper 16 bits containing modification write masks
    } bits;

    u32 raw;

    enum PPGTTMode : u32 {
        PPGTT_DISABLE = 0,
        PPGTT_ENABLE  = 1,
    };

    enum VirtualAddressingMode : u32 {
        VA_32BIT_DISABLE = 0,
        VA_64BIT_ENABLE  = 1,
    };

    /**
     * @brief Helper to prepare a write payload that modifies the Execlist status.
     */
    constexpr void set_execlist_enable(bool enable) {
        execlist_enable = enable ? 1 : 0;
        mask_execlist_enable = 1;
    }

    /**
     * @brief Helper to prepare a write payload that modifies PPGTT mode.
     */
    constexpr void set_ppgtt_enable(bool enable) {
        ppgtt_enable = enable ? 1 : 0;
        mask_ppgtt_enable = 1;
    }

    /**
     * @brief Helper to prepare a write payload that modifies 64-bit addressing mode.
     */
    constexpr void set_64bit_va_enable(bool enable) {
        va64_bit_enable = enable ? 1 : 0;
        mask_va64_bit_enable = 1;
    }
};

static_assert(sizeof(GFX_MODE) == 4, "GFX_MODE register must be 32 bits");


#endif  // VESPERAOS_EXECLIST_REGS_H
