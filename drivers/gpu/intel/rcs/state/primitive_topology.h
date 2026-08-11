// primitive_topology.h
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

#ifndef VESPERAOS_PRIMITIVE_TOPOLOGY_H
#define VESPERAOS_PRIMITIVE_TOPOLOGY_H

#include <vespera/types.h>

/**
 * @brief 3D_Prim_Topo_Type — 6-bit primitive topology type encoding used by RenderCS.
 *
 * Defines the encoding of the Primitive Topology Type field for the 3D pipeline.
 *
 * @see IHD-OS-KBL-Vol 2b-1.17, p. 6 (3D_Prim_Topo_Type)
 */
enum PrimTopoType : u32 {
    PRIM_3D_POINTLIST            = 0x01,
    PRIM_3D_LINELIST             = 0x02,
    PRIM_3D_LINESTRIP            = 0x03,
    PRIM_3D_TRILIST              = 0x04,
    PRIM_3D_TRISTRIP             = 0x05,
    PRIM_3D_TRIFAN               = 0x06,
    PRIM_3D_QUADLIST             = 0x07,  ///< Converted to POLYGON topology at the beginning of the 3D pipeline
    PRIM_3D_QUADSTRIP            = 0x08,  ///< Converted to POLYGON topology at the beginning of the 3D pipeline
    PRIM_3D_LINELIST_ADJ         = 0x09,
    PRIM_3D_LINESTRIP_ADJ        = 0x0A,
    PRIM_3D_TRILIST_ADJ          = 0x0B,
    PRIM_3D_TRISTRIP_ADJ         = 0x0C,
    PRIM_3D_TRISTRIP_REVERSE     = 0x0D,
    PRIM_3D_POLYGON              = 0x0E,
    PRIM_3D_RECTLIST             = 0x0F,
    PRIM_3D_LINELOOP             = 0x10,  ///< Converted to LINESTRIP topology at the beginning of the 3D pipeline
    PRIM_3D_POINTLIST_BF         = 0x11,
    PRIM_3D_LINESTRIP_CONT       = 0x12,
    PRIM_3D_LINESTRIP_BF         = 0x13,
    PRIM_3D_LINESTRIP_CONT_BF    = 0x14,
    PRIM_3D_TRIFAN_NOSTIPPLE     = 0x16,
    PRIM_3D_PATCHLIST_1          = 0x20,  ///< List of 1-vertex patches
    PRIM_3D_PATCHLIST_2          = 0x21,
    PRIM_3D_PATCHLIST_3          = 0x22,
    PRIM_3D_PATCHLIST_4          = 0x23,
    PRIM_3D_PATCHLIST_5          = 0x24,
    PRIM_3D_PATCHLIST_6          = 0x25,
    PRIM_3D_PATCHLIST_7          = 0x26,
    PRIM_3D_PATCHLIST_8          = 0x27,
    PRIM_3D_PATCHLIST_9          = 0x28,
    PRIM_3D_PATCHLIST_10         = 0x29,
    PRIM_3D_PATCHLIST_11         = 0x2A,
    PRIM_3D_PATCHLIST_12         = 0x2B,
    PRIM_3D_PATCHLIST_13         = 0x2C,
    PRIM_3D_PATCHLIST_14         = 0x2D,
    PRIM_3D_PATCHLIST_15         = 0x2E,
    PRIM_3D_PATCHLIST_16         = 0x2F,
    PRIM_3D_PATCHLIST_17         = 0x30,
    PRIM_3D_PATCHLIST_18         = 0x31,
    PRIM_3D_PATCHLIST_19         = 0x32,
    PRIM_3D_PATCHLIST_20         = 0x33,
    PRIM_3D_PATCHLIST_21         = 0x34,
    PRIM_3D_PATCHLIST_22         = 0x35,
    PRIM_3D_PATCHLIST_23         = 0x36,
    PRIM_3D_PATCHLIST_24         = 0x37,
    PRIM_3D_PATCHLIST_25         = 0x38,
    PRIM_3D_PATCHLIST_26         = 0x39,
    PRIM_3D_PATCHLIST_27         = 0x3A,
    PRIM_3D_PATCHLIST_28         = 0x3B,
    PRIM_3D_PATCHLIST_29         = 0x3C,
    PRIM_3D_PATCHLIST_30         = 0x3D,
    PRIM_3D_PATCHLIST_31         = 0x3E,
    PRIM_3D_PATCHLIST_32         = 0x3F,  ///< List of 32-vertex patches
};

#endif  // VESPERAOS_PRIMITIVE_TOPOLOGY_H