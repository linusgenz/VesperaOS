/**
 * @file IRenderDriver.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 29.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VESPERAOS_IRENDERDRIVER_H
#define VESPERAOS_IRENDERDRIVER_H

#include <vespera/types.h>

struct GlyphRun {
    const char* text;
    u32 length;

    u32 px;
    u32 py;

    u32 fg;
    u32 bg;
};

class IRenderDriver {
   public:
    virtual ~IRenderDriver() = default;

    virtual void draw_glyph_run(const GlyphRun& run) = 0;

    virtual bool fill_rect(u32 px, u32 py, u32 w, u32 h, u32 colour) = 0;

    virtual bool blit_buffer(
        const void* pixels, u32 buffer_width, u32 buffer_height, u32 dst_x, u32 dst_y
    ) = 0;

    virtual bool scroll_pixels(int dy) = 0;

    [[nodiscard]] virtual u32 screen_width_px() const = 0;
    [[nodiscard]] virtual u32 screen_height_px() const = 0;
    [[nodiscard]] virtual u32 bytes_per_scanline() const = 0;
};

#endif  // VESPERAOS_IRENDERDRIVER_H
