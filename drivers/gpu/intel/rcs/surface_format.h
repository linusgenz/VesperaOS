// surface_format.h
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


#ifndef VESPERAOS_SURFACE_FORMAT_H
#define VESPERAOS_SURFACE_FORMAT_H

#include <vespera/types.h>

/**
 * @brief SURFACE_FORMAT — 9-bit format encoding shared by the Sampling
 *        Engine, Data Port, and Vertex Fetch unit.
 *
 * Used by VERTEX_ELEMENT_STATE::source_element_format (see
 * cmd_vertex_elements.h) and, later, by SURFACE_STATE for textures/render
 * targets. This is a deliberately partial subset of the full ~250-entry
 * table — only formats this driver actually emits. Extend as needed,
 * copying the exact 9-bit value from the PRM table rather than deriving it.
 *
 * @see IHD-OS-KBL-Vol 2b-1.17, pp. 41-46 (SURFACE_FORMAT)
 */
enum SurfaceFormat : u32 {
    SURFACE_FORMAT_R32G32B32A32_FLOAT = 0x000,  ///< 128 BPE
    SURFACE_FORMAT_R32G32B32_FLOAT = 0x040,     ///< 96 BPE — used for float3 vertex positions
    SURFACE_FORMAT_R32G32_FLOAT = 0x085,        ///< 64 BPE
    SURFACE_FORMAT_R32_FLOAT = 0x0D8,           ///< 32 BPE
    SURFACE_FORMAT_R8G8B8A8_UNORM = 0x0C7,      ///< 32 BPE — common render target / texture format
    SURFACE_FORMAT_B8G8R8A8_UNORM = 0x0C0,      ///< 32 BPE — common display/render target format
};

#endif  // VESPERAOS_SURFACE_FORMAT_H
