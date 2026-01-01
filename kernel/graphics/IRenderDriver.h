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

#include <cstdint>

struct GlyphRun
{
    const char* text;
    uint32_t length;

    uint32_t px;
    uint32_t py;

    uint32_t fg;
    uint32_t bg;
};

class IRenderDriver
{
public:
    virtual ~IRenderDriver() = default;

    virtual void draw_glyph_run(const GlyphRun& run) = 0;

    virtual bool fill_rect(
        uint32_t px,
        uint32_t py,
        uint32_t w,
        uint32_t h,
        uint32_t colour
    ) = 0;

    virtual bool blit_buffer(
        const void* pixels,
        uint32_t buffer_width,
        uint32_t buffer_height,
        uint32_t dst_x,
        uint32_t dst_y
    ) = 0;

    virtual bool scroll_pixels(int dy) = 0;

    [[nodiscard]] virtual uint32_t screen_width_px() const = 0;
    [[nodiscard]] virtual uint32_t screen_height_px() const = 0;
    [[nodiscard]] virtual uint32_t bytes_per_scanline() const = 0;
};

#endif //VESPERAOS_IRENDERDRIVER_H
