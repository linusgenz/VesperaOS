// display_reg_addr.h
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

#ifndef VESPERAOS_DISPLAY_REG_ADDR_H
#define VESPERAOS_DISPLAY_REG_ADDR_H

#include <vespera/types.h>

#include "display_surface.h"

namespace gpu::intel::display {
    // Plane registers: the *_1_A constants in display_regs.h are the base,
    // every other pipe/plane is a fixed offset away.
    //   pipe  stride 0x1000 (A 0x70xxx, B 0x71xxx, C 0x72xxx)
    //   plane stride 0x100  (1 0x?x1xx, 2 0x?x2xx, 3 0x?x3xx)
    constexpr u32 PIPE_REG_STRIDE = 0x1000;
    constexpr u32 PLANE_REG_STRIDE = 0x100;

    [[nodiscard]] constexpr u32 plane_reg(const PipeId pipe, const PlaneId plane, const u32 reg_1a) {
        return reg_1a + static_cast<u32>(pipe) * PIPE_REG_STRIDE + static_cast<u32>(plane) * PLANE_REG_STRIDE;
    }

    // DE pipe interrupt registers (interrupt_regs.h): the A/B/C blocks are
    // 0x10 apart. Pass the DE_PIPE_A_* constant as `reg_a`.
    constexpr u32 DE_PIPE_REG_STRIDE = 0x10;

    [[nodiscard]] constexpr u32 de_pipe_reg(const PipeId pipe, const u32 reg_a) {
        return reg_a + static_cast<u32>(pipe) * DE_PIPE_REG_STRIDE;
    }

    // DE_PIPE_INTERRUPT bit masks (same layout as the DE_PIPE_INTERRUPT bitfield,
    // as masks so the per-plane bits can be addressed by index).
    constexpr u32 DE_PIPE_VBLANK_MASK = 1u << 0;

    [[nodiscard]] constexpr u32 de_pipe_flip_done_mask(const PlaneId plane) {
        return 1u << (3u + static_cast<u32>(plane)); // plane1_flip_done = bit 3
    }

    constexpr u32 DE_PIPE_FLIP_DONE_ALL_MASK = (1u << 3) | (1u << 4) | (1u << 5);

    [[nodiscard]] constexpr u32 cursor_reg(const PipeId pipe, const u32 reg_a) {
        return reg_a + static_cast<u32>(pipe) * PIPE_REG_STRIDE;
    }

    constexpr u32 CURSOR_PAL_STRIDE = 4;
    constexpr u32 CURSOR_PAL_COUNT = 4;

    [[nodiscard]] constexpr u32 cursor_pal_reg(const PipeId pipe, const u32 index) {
        return CUR_PAL_A_0 + static_cast<u32>(pipe) * PIPE_REG_STRIDE + index * CURSOR_PAL_STRIDE;
    }
} // namespace gpu::intel::display

#endif  // VESPERAOS_DISPLAY_REG_ADDR_H
