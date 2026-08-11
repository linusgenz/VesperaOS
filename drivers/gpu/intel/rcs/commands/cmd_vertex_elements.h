// cmd_vertex_elements.h
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

#ifndef VESPERAOS_CMD_VERTEX_ELEMENTS_H
#define VESPERAOS_CMD_VERTEX_ELEMENTS_H

#include <vespera/types.h>

#include "cmd_scissor.h"

/**
 * @brief 3D_Vertex_Component_Control encoding (3 bits).
 *
 * Controls what each of a vertex element's 4 output components (stored into
 * the vertex URB entry) is populated with.
 *
 * @note Once a component uses VFCOMP_NOSTORE, every higher-numbered
 *       component in the same element MUST also use VFCOMP_NOSTORE — no
 *       "holes" are allowed within a vertex element.
 * @note VFCOMP_NOSTORE is NOT valid for Component 0.
 *
 * @see IHD-OS-KBL-Vol 2b-1.17 (3D_Vertex_Component_Control)
 */
enum VertexComponentControl : u32 {
    VFCOMP_NOSTORE     = 0, ///< Don't store this component (not valid for Component 0)
    VFCOMP_STORE_SRC   = 1, ///< Store the corresponding format-converted source component
    VFCOMP_STORE_0     = 2, ///< Store 0 (0.0f if read as float)
    VFCOMP_STORE_1_FP  = 3, ///< Store 1.0f
    VFCOMP_STORE_1_INT = 4, ///< Store 0x1
    // 5-6 reserved
    VFCOMP_STORE_PID = 7, ///< Store Primitive ID (legacy — avoid in new code, see PRM note)
};

/**
 * @brief VERTEX_ELEMENT_STATE structure (2 DWords / 64 bits).
 *
 * Describes how one vertex element's source data (from a VERTEX_BUFFER_STATE
 * buffer) is fetched, format-converted, and expanded into up to 4 URB output
 * components. Embedded 1-to-34 times inside a 3DSTATE_VERTEX_ELEMENTS
 * command.
 *
 * @note Element[0] must be valid; validity must be contiguous from
 *       Element[0] up to the last valid element (no gaps).
 * @note Only trailing components may use something other than
 *       VFCOMP_STORE_SRC — e.g. Component 0-2 = STORE_SRC (XYZ from the
 *       buffer), Component 3 = STORE_1_FP (W=1.0) is valid; the reverse
 *       order is not.
 *
 * @see IHD-OS-KBL-Vol 2d-1.17, pp. 732-735 (VERTEX_ELEMENT_STATE)
 */
union VERTEX_ELEMENT_STATE {
    struct {
        // ====================================================================
        // DWord 0 (Bits 31:0)
        // ====================================================================
        u32 source_element_offset : 12; ///< [11:0]  Byte offset
        u32 reserved0_12          : 3;  ///< [14:12] MBZ
        u32 edge_flag_enable      : 1;  ///< [15]    EdgeFlag
        u32 source_element_format : 9;  ///< [24:16] SURFACE_FORMAT
        u32 valid                 : 1;  ///< [25]    Valid bit
        u32 vertex_buffer_index   : 6;  ///< [31:26] VB slot

        // ====================================================================
        // DWord 1 (Bits 31:0)
        // ====================================================================
        u32 reserved1_0         : 16; ///< [15:0]  MBZ (Gefehlt!)
        u32 component_3_control : 3;  ///< [18:16] Component 3 Control
        u32 reserved1_19        : 1;  ///< [19]    MBZ
        u32 component_2_control : 3;  ///< [22:20] Component 2 Control
        u32 reserved1_23        : 1;  ///< [23]    MBZ
        u32 component_1_control : 3;  ///< [26:24] Component 1 Control
        u32 reserved1_27        : 1;  ///< [27]    MBZ
        u32 component_0_control : 3;  ///< [30:28] Component 0 Control
        u32 reserved1_31        : 1;  ///< [31]    MBZ
    } __attribute__((packed));

    u32 raw[2];

    /**
     * @brief Creates a VERTEX_ELEMENT_STATE that reads all 4 components
     *        (XYZW-style) from the source buffer via VFCOMP_STORE_SRC.
     *
     * @param vb_index      Vertex buffer slot this element is sourced from.
     * @param offset_bytes  Byte offset of this element within the vertex.
     * @param format        SURFACE_FORMAT of the source data (e.g. R32G32B32_FLOAT).
     */
    [[nodiscard]] static constexpr VERTEX_ELEMENT_STATE create_store_src(
        u32 vb_index, u32 offset_bytes, u32 format
    ) {
        VERTEX_ELEMENT_STATE el{};
        el.vertex_buffer_index = vb_index;
        el.valid = 1;
        el.source_element_format = format;
        el.source_element_offset = offset_bytes;
        el.component_0_control = VFCOMP_STORE_SRC;
        el.component_1_control = VFCOMP_STORE_SRC;
        el.component_2_control = VFCOMP_STORE_SRC;
        el.component_3_control = VFCOMP_STORE_SRC;
        return el;
    }

    /**
     * @brief Creates a VERTEX_ELEMENT_STATE for a 3-component source (e.g.
     *        R32G32B32_FLOAT position) with a constant W=1.0 appended —
     *        only trailing components may deviate from VFCOMP_STORE_SRC.
     *
     * @param vb_index      Vertex buffer slot this element is sourced from.
     * @param offset_bytes  Byte offset of this element within the vertex.
     * @param format        SURFACE_FORMAT of the source data (e.g. R32G32B32_FLOAT).
     */
    [[nodiscard]] static constexpr VERTEX_ELEMENT_STATE create_xyz_w1(
        u32 vb_index, u32 offset_bytes, u32 format
    ) {
        VERTEX_ELEMENT_STATE el{};
        el.vertex_buffer_index = vb_index;
        el.valid = 1;
        el.source_element_format = format;
        el.source_element_offset = offset_bytes;
        el.component_0_control = VFCOMP_STORE_SRC;  // X
        el.component_1_control = VFCOMP_STORE_SRC;  // Y
        el.component_2_control = VFCOMP_STORE_SRC;  // Z
        el.component_3_control = VFCOMP_STORE_1_FP; // W = 1.0f
        return el;
    }
};

static_assert(sizeof(VERTEX_ELEMENT_STATE) == 8, "VERTEX_ELEMENT_STATE must be exactly 2 DWords (8 bytes)");

/**
 * @brief 3DSTATE_VERTEX_ELEMENTS command header (variable length).
 *
 * Carries 1 to 34 embedded VERTEX_ELEMENT_STATE structures (2 DWords each).
 * The caller writes this header via ring_write_cmd(), then each
 * VERTEX_ELEMENT_STATE immediately after, also via ring_write_cmd().
 *
 * @note DWord Length formula per PRM: Vertex Element Count = (DWord Count + 1) / 2,
 *       i.e. DWord Length = 2*n - 1, where n = number of VERTEX_ELEMENT_STATE
 *       structures included (excludes DWords 0,1 of this header itself).
 * @note Element[0] must always be valid; at least one element must be
 *       defined before any 3DPRIMITIVE command.
 *
 * @see IHD-OS-KBL-Vol 2a-1.17, pp. 242 (3DSTATE_VERTEX_ELEMENTS)
 */
union VERTEX_ELEMENTS_HEADER {
    enum CommandSubOpcode : u32 {
        SUBOP_3DSTATE_VERTEX_ELEMENTS = 0x09,
    };

    struct {
        u32 dword_length : 8; ///< [7:0]   = 2*n - 1 (n = number of elements included)
        u32 reserved0_8  : 8; ///< [15:8]  MBZ
        u32 sub_opcode   : 8; ///< [23:16] Default: 0x09 (3DSTATE_VERTEX_ELEMENTS)
        u32 opcode       : 3; ///< [26:24] Default: 0x0  (3DSTATE_PIPELINED)
        u32 sub_type     : 2; ///< [28:27] Default: 0x3  (GFXPIPE_3D)
        u32 command_type : 3; ///< [31:29] Default: 0x3  (GFXPIPE)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Creates the command header for `num_elements` embedded
     *        VERTEX_ELEMENT_STATE structures.
     *
     * @param num_elements Number of VERTEX_ELEMENT_STATE structures that
     *                      will immediately follow this header in the ring
     *                      (1-34).
     */
    [[nodiscard]] static constexpr VERTEX_ELEMENTS_HEADER create(u32 num_elements) {
        VERTEX_ELEMENTS_HEADER cmd{};
        cmd.command_type = CMD_GFXPIPE;
        cmd.sub_type = GFXPIPE_3D;
        cmd.opcode = OPCODE_3DSTATE_PIPELINED;
        cmd.sub_opcode = SUBOP_3DSTATE_VERTEX_ELEMENTS;
        cmd.dword_length = (2 * num_elements) - 1;
        return cmd;
    }
};

static_assert(sizeof(VERTEX_ELEMENTS_HEADER) == 4, "VERTEX_ELEMENTS_HEADER must be 32 bits");

#endif  // VESPERAOS_CMD_VERTEX_ELEMENTS_H
