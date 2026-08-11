// blend_state.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.08.26.
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

#ifndef VESPERAOS_CMD_BLEND_STATE_H
#define VESPERAOS_CMD_BLEND_STATE_H

#include <vespera/types.h>

#include "cmd_common.h"

/**
 * @brief BLEND_STATE_ENTRY structure (2 DWords / 64 bits).
 *
 * Per-render-target blend configuration. An array of up to 8 of these
 * follows the common DWord0 inside BLEND_STATE, spaced 2 DWords apart.
 * The 3-bit Render Target Index in the Render Target Write message header
 * selects which entry is used for a given RT.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 50-54 (BLEND_STATE_ENTRY)
 */
union BLEND_STATE_ENTRY {
    enum ColorBufferBlendFactor : u32 {
        BLENDFACTOR_ONE                  = 0x01,
        BLENDFACTOR_SRC_COLOR            = 0x02,
        BLENDFACTOR_SRC_ALPHA            = 0x03,
        BLENDFACTOR_DST_ALPHA            = 0x04,
        BLENDFACTOR_DST_COLOR            = 0x05,
        BLENDFACTOR_SRC_ALPHA_SATURATE   = 0x06,
        BLENDFACTOR_CONST_COLOR          = 0x07,
        BLENDFACTOR_CONST_ALPHA          = 0x08,
        BLENDFACTOR_SRC1_COLOR           = 0x09,
        BLENDFACTOR_SRC1_ALPHA           = 0x0A,
        BLENDFACTOR_ZERO                 = 0x11,
        BLENDFACTOR_INV_SRC_COLOR        = 0x12,
        BLENDFACTOR_INV_SRC_ALPHA        = 0x13,
        BLENDFACTOR_INV_DST_ALPHA        = 0x14,
        BLENDFACTOR_INV_DST_COLOR        = 0x15,
        BLENDFACTOR_INV_CONST_COLOR      = 0x17,
        BLENDFACTOR_INV_CONST_ALPHA      = 0x18,
        BLENDFACTOR_INV_SRC1_COLOR       = 0x19,
        BLENDFACTOR_INV_SRC1_ALPHA       = 0x1A,
    };

    enum ColorBufferBlendFunction : u32 {
        BLENDFUNCTION_ADD          = 0x0,
        BLENDFUNCTION_SUBTRACT     = 0x1,
        BLENDFUNCTION_REVERSE_SUBTRACT = 0x2,
        BLENDFUNCTION_MIN         = 0x3,
        BLENDFUNCTION_MAX         = 0x4,
    };

    struct {
        // DWord 0
        u32 write_disable_blue : 1;               ///< [0]     1 = writes to Blue suppressed
        u32 write_disable_green : 1;               ///< [1]     1 = writes to Green suppressed
        u32 write_disable_red : 1;                 ///< [2]     1 = writes to Red suppressed
        u32 write_disable_alpha : 1;               ///< [3]     1 = writes to Alpha suppressed
        u32 reserved0_4 : 1;                       ///< [4]     MBZ
        u32 alpha_blend_function : 3;               ///< [7:5]   ColorBufferBlendFunction enum
        u32 destination_alpha_blend_factor : 5;     ///< [12:8]  ColorBufferBlendFactor enum
        u32 source_alpha_blend_factor : 5;          ///< [17:13] ColorBufferBlendFactor enum
        u32 color_blend_function : 3;               ///< [20:18] ColorBufferBlendFunction enum
        u32 destination_blend_factor : 5;           ///< [25:21] ColorBufferBlendFactor enum
        u32 source_blend_factor : 5;                ///< [30:26] ColorBufferBlendFactor enum
        u32 color_buffer_blend_enable : 1;          ///< [31]    Enable Color Buffer (alpha) Blending for this RT

        // DWord 1
        u32 post_blend_color_clamp_enable : 1;      ///< [0]     Clamp blend output to RT format range
        u32 pre_blend_color_clamp_enable : 1;       ///< [1]     Clamp all blend inputs to Color Clamp Range
        u32 color_clamp_range : 2;                  ///< [3:2]   0=UNORM [0,1], 1=SNORM [-1,1], 2=RTFORMAT
        u32 pre_blend_source_only_clamp_enable : 1; ///< [4]     Clamp source(s) only, prior to blend
        u32 reserved1_5 : 26;                       ///< [30:5]  MBZ
        u32 logic_op_enable : 1;                    ///< [31]    Enable LogicOp (mutually exclusive w/ blending)
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a default-initialized (all-zero) BLEND_STATE_ENTRY.
     *
     * Matches the PRM reset default: blending disabled, LogicOp disabled,
     * no clamping, and all color/alpha writes enabled (write-disable bits
     * are active-low, so 0 = writable).
     */
    [[nodiscard]] static constexpr BLEND_STATE_ENTRY create() {
        return BLEND_STATE_ENTRY{};
    }

    /**
     * @brief Creates a BLEND_STATE_ENTRY for simple opaque rendering.
     *
     * Blending and LogicOp disabled, all RGBA channels writable. This is
     * the standard "just write what the PS outputs" configuration and
     * matches the all-zero PRM default exactly — provided as a named
     * entry point for clarity at call sites.
     */
    [[nodiscard]] static constexpr BLEND_STATE_ENTRY create_opaque_writeable() {
        BLEND_STATE_ENTRY entry = create();
        entry.color_buffer_blend_enable = 0;
        entry.logic_op_enable = 0;
        entry.write_disable_red = 0;
        entry.write_disable_green = 0;
        entry.write_disable_blue = 0;
        entry.write_disable_alpha = 0;
        return entry;
    }
};

static_assert(sizeof(BLEND_STATE_ENTRY) == 8, "BLEND_STATE_ENTRY must be exactly 2 DWords (8 bytes)");

/**
 * @brief BLEND_STATE structure (17 DWords / 544 bits).
 *
 * Memory-resident (Dynamic State Base Address-relative, 64-byte aligned),
 * NOT part of the command stream — 3DSTATE_BLEND_STATE_POINTERS only
 * points at it. DWord 0 holds settings shared across all render targets
 * (AlphaToCoverage, Alpha Test, dithering); DWords 1..16 hold up to 8
 * BLEND_STATE_ENTRY elements, one per render target.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 48-49 (BLEND_STATE)
 */
union BLEND_STATE {
    enum CompareFunction : u32 {
        COMPARE_ALWAYS   = 0x0,
        COMPARE_NEVER    = 0x1,
        COMPARE_LESS     = 0x2,
        COMPARE_EQUAL    = 0x3,
        COMPARE_LEQUAL   = 0x4,
        COMPARE_GREATER  = 0x5,
        COMPARE_NOTEQUAL = 0x6,
        COMPARE_GEQUAL   = 0x7,
    };

    enum ColorClampRange : u32 {
        COLORCLAMP_UNORM    = 0x0,
        COLORCLAMP_SNORM    = 0x1,
        COLORCLAMP_RTFORMAT = 0x2,
    };

    static constexpr u32 MAX_RENDER_TARGETS = 8;

    struct {
        u32 reserved0_0 : 19;                    ///< [18:0]  MBZ
        u32 y_dither_offset : 2;                 ///< [20:19] U2
        u32 x_dither_offset : 2;                 ///< [22:21] U2
        u32 color_dither_enable : 1;             ///< [23]    Dither colors before Color Buffer write
        u32 alpha_test_function : 3;             ///< [26:24] CompareFunction enum
        u32 alpha_test_enable : 1;               ///< [27]    Enable AlphaTest (requires float alpha PS output)
        u32 alpha_to_coverage_dither_enable : 1; ///< [28]    Dither AlphaToCoverage mask by screen coords
        u32 alpha_to_one_enable : 1;             ///< [29]    Force Source0 Alpha to 1.0 after AlphaToCoverage
        u32 independent_alpha_blend_enable : 1;  ///< [30]    Alpha components blended independently of color
        u32 alpha_to_coverage_enable : 1;        ///< [31]    Convert Source0 Alpha to a sample coverage mask

        // DWords 1..16 — up to 8 BLEND_STATE_ENTRY elements (2 DWords each)
        BLEND_STATE_ENTRY entries[MAX_RENDER_TARGETS];
    } __attribute__((packed));

    u32 raw[1 + 2 * MAX_RENDER_TARGETS];

    /**
     * @brief Creates a default-initialized (all-zero) BLEND_STATE.
     *
     * Matches the PRM reset default exactly (all 17 DWords 0x00000000):
     * AlphaToCoverage/AlphaTest/dithering disabled, and every RT entry
     * defaults to blending disabled with all channels writable.
     */
    [[nodiscard]] static constexpr BLEND_STATE create() {
        return BLEND_STATE{};
    }

    /**
     * @brief Creates a BLEND_STATE for a single opaque render target.
     *
     * RT 0 gets create_opaque_writeable() (blending off, RGBA writable);
     * RTs 1..7 stay at the all-zero default since they're unused for a
     * single-render-target.
     */
    [[nodiscard]] static constexpr BLEND_STATE create_single_rt_opaque() {
        BLEND_STATE state = create();
        state.entries[0] = BLEND_STATE_ENTRY::create_opaque_writeable();
        return state;
    }
};

static_assert(sizeof(BLEND_STATE) == 68, "BLEND_STATE must be exactly 17 DWords (68 bytes)");

#endif  // VESPERAOS_CMD_BLEND_STATE_H

