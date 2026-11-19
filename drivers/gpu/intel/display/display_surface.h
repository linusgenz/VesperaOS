// display_surface.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.11.26.
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

#ifndef VESPERAOS_DISPLAY_SURFACE_H
#define VESPERAOS_DISPLAY_SURFACE_H

#include <vespera/mm/addr.h>
#include <vespera/types.h>

#include <gpu/intel/regs/display_regs.h>

namespace gpu::intel::display {

    // =========================================================================
    // Identifiers
    // =========================================================================

    enum class PipeId : u8 {
        A = 0,
        B = 1,
        C = 2,
    };

    enum class PlaneId : u8 {
        Plane1 = 0,
        Plane2 = 1,
        Plane3 = 2,
    };

    constexpr u32 PIPE_COUNT = 3;
    constexpr u32 PLANES_PER_PIPE = 3;

    // =========================================================================
    // Surface / tiling
    // =========================================================================

    /// Shared by the display planes and the blitter (BCS), which used to own
    /// this enum as bcs::TileMode.
    enum class TileMode : u8 {
        Linear = 0,
        X = 1,
        Y = 2,
    };

    constexpr u32 SCANOUT_BYTES_PER_PIXEL = 4;

    /// PLANE_SURF: "Allocate 136 extra PTEs beyond the end of the displayed
    /// surface" (see display_regs.h). We back them with real pages for now,
    /// which costs ~544 KB per scanout buffer but keeps the GGTT code simple.
    constexpr u32 SCANOUT_PTE_PADDING_PAGES = 136;

    constexpr u32 PLANE_MAX_WIDTH = 8192;
    constexpr u32 PLANE_MAX_HEIGHT = 4096;

    [[nodiscard]] constexpr PLANE_CTL::TiledSurface to_plane_tiling(const TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return PLANE_CTL::TILING_X;
            case TileMode::Y:
                return PLANE_CTL::TILING_Y;
            default:
                return PLANE_CTL::TILING_LINEAR;
        }
    }

    /// Unit of PLANE_STRIDE for this tiling (see PLANE_STRIDE in display_regs.h).
    [[nodiscard]] constexpr u32 pitch_alignment_bytes(const TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return 512;
            case TileMode::Y:
                return 128;
            default:
                return 64;
        }
    }

    [[nodiscard]] constexpr u32 tile_height_rows(const TileMode mode) {
        switch (mode) {
            case TileMode::X:
                return 8;
            case TileMode::Y:
                return 32;
            default:
                return 1;
        }
    }

    /// PLANE_SURF minimum alignment: Linear/Tile X 256 KB, Tile Y 1 MB.
    [[nodiscard]] constexpr u32 surface_alignment_bytes(const TileMode mode) {
        return mode == TileMode::Y ? (1u << 20) : (256u << 10);
    }

    // =========================================================================
    // PlaneConfig — everything DisplayPlane::commit() programs in one go
    // =========================================================================

    struct PlaneConfig {
        gfx_addr_t surface{};

        u32 width = 0;   ///< PLANE_SIZE width in pixels
        u32 height = 0;  ///< PLANE_SIZE height in lines
        u32 src_x = 0;   ///< PLANE_OFFSET x (panning inside the surface)
        u32 src_y = 0;   ///< PLANE_OFFSET y
        u32 dst_x = 0;   ///< PLANE_POS x (position on the pipe)
        u32 dst_y = 0;   ///< PLANE_POS y

        u32 pitch_bytes = 0;
        PLANE_CTL::PixelFormat format = PLANE_CTL::FMT_RGB_8_8_8_8;
        TileMode tiling = TileMode::Linear;
        PLANE_CTL::Rotation rotation = PLANE_CTL::ROTATE_0;
    };

}  // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_SURFACE_H
