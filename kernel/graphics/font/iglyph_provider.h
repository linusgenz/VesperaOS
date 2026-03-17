// iglyph_provider.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.03.26.
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
#ifndef VESPERAOS_IGLYPH_PROVIDER_H
#define VESPERAOS_IGLYPH_PROVIDER_H

#include <vespera/types.h>

struct RenderedGlyph {
    u8*  bitmap;
    u32  width;
    u32  height;
    i32  bearing_x;
    i32  bearing_y;
    i32  advance;
};

class IGlyphProvider {
public:
    virtual ~IGlyphProvider() = default;

    // Render a glyph for a Unicode codepoint.
    // Return value: pointer to internal bitmap (valid until the next call
    // or Destroy). nullptr if the codepoint is not supported.
    virtual const RenderedGlyph* get_glyph(u32 codepoint) = 0;

    [[nodiscard]] virtual u32 line_height()  const = 0;  // Line height in pixels
    [[nodiscard]] virtual u32 baseline()     const = 0;  // Distance from top edge to baseline
    [[nodiscard]] virtual u32 char_width()   const = 0;  // Nominal character width (for monospace fonts)
    [[nodiscard]] virtual bool is_monospace() const = 0;
};

#endif  // VESPERAOS_IGLYPH_PROVIDER_H
