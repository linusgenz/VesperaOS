/**
 * @file terminal.h
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
#ifndef VESPERAOS_TERMINAL_H
#define VESPERAOS_TERMINAL_H

#include <vespera/graphics.h>
#include <vespera/types.h>

#include "../../kernel/graphics/IRenderDriver.h"

class ScrollbackBuffer;
class IGlyphProvider;
class GlyphCache;
class Terminal {
    IRenderDriver* drv_ = nullptr;
    IGlyphProvider* glyphs_ = nullptr;
    GlyphCache* cache_ = nullptr;

    u32 char_w_{};
    u32 char_h_{};

    usize cols_{};
    usize rows_{};

    u32 cx_ = 0;  // Column cursor

    u32 fg_ = 0xFFFFFFFF;
    u32 bg_ = 0x00000000;

    ScrollbackBuffer* sb_;

   public:
    Terminal(IRenderDriver* d, u32 char_width, u32 char_height);
    ~Terminal();

    void scrollback_up(usize lines = 3) const;
    void scrollback_down(usize lines = 3) const;
    void scrollback_to_bottom() const;
    [[nodiscard]] bool is_at_bottom() const;
    [[nodiscard]] usize visible_rows() const {
        return rows_;
    }

    void set_glyph_provider(IGlyphProvider* provider);

    void set_colour(u32 new_fg, u32 new_bg);
    void set_cursor(u32 x, u32 y);
    void put_char(char c);
    void put_char_fast(char c);
    void print(const char* s);
    void put_codepoint(uint32_t cp);
    void clear();
    void clear_char();
    void new_line();
    void flush() const;

   private:
    void draw_cell(u32 cx, u32 cy) const;
    void advance();
    void scroll() const;
};

extern font_t* system_font;

#endif  // VESPERAOS_TERMINAL_H
