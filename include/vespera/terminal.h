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

#include <vespera/graphics/IRenderDriver.h>
#include <vespera/types.h>

#include "graphics/psf.h"

struct DisplayBackend;
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

    u32 cursor_col_ = 0;
    u32 cursor_row_ = 0;

    u32 fg_ = 0xFFFFFFFF;
    u32 bg_ = 0x00000000;

    ScrollbackBuffer* sb_;

    bool cursor_visible_ = false;
    bool cursor_blink_on_ = true;
    u32 blink_idle_ticks_ = 0;
    u32 blink_pause_ticks_ =
        2; /* this is not based on the apic interval, but on the interval in which tick_cursor gets called*/

    u32 screen_w_ = 0;
    u32 screen_h_ = 0;
    u32* screen_buf_ = nullptr;

    mutable bool has_dirty_ = false;
    mutable u32 dirty_x0_ = 0, dirty_y0_ = 0;
    mutable u32 dirty_x1_ = 0, dirty_y1_ = 0;

    void fill_buf_rect(u32 x, u32 y, u32 w, u32 h, u32 color);
    void mark_dirty_px(u32 x, u32 y, u32 w, u32 h);
    void commit_dirty();
    void draw_cell(u32 cx, u32 cy);

    void draw_cursor() const;
    void erase_cursor_under() const;

    void draw_cell(u32 cx, u32 cy) const;
    void advance();

    void cursor_activity();

   public:
    Terminal(IRenderDriver* d, u32 char_width, u32 char_height);
    ~Terminal();

    void draw_overlay_mouse_cursor(const u8* bitmap, const point_t pos, const u32 color);

    void clear_mouse_cursor(const u8* bitmap, const point_t pos);

    void on_backend_changed(DisplayBackend backend);

    void scrollback_up(usize lines = 3);
    void scrollback_down(usize lines = 3);
    void scrollback_to_bottom();
    [[nodiscard]] bool is_at_bottom() const;
    [[nodiscard]] usize visible_rows() const {
        return rows_;
    }
    [[nodiscard]] usize visible_cols() const {
        return cols_;
    }

    void erase_in_line(int mode, u32 col, u32 row) const;
    void erase_in_display(int mode, u32 col, u32 row);
    void draw_cursor();
    void erase_cursor_under();

    void set_glyph_provider(IGlyphProvider* provider);

    void tick_cursor();
    void set_cursor_visible(bool v);

    void set_colour(u32 new_fg, u32 new_bg);
    void set_cursor(u32 x, u32 y);
    void put_char(char c);
    void put_char_fast(char c);
    void print(const char* s);
    void put_codepoint(uint32_t cp);
    void clear();
    void clear_char();
    void new_line();
    void erase_in_line(int mode, u32 col, u32 row);
    void flush();
};

extern PsfFont* system_font;

#endif  // VESPERAOS_TERMINAL_H
