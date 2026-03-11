/**
 * @file terminal.cpp
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 30.12.25.
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

#include <vespera/terminal.h>

#include "IRenderDriver.h"
#include <vespera/graphics.h>

font_t* system_font = nullptr;
Terminal* global_terminal = nullptr;

Terminal::Terminal(IRenderDriver* d, u32 char_width, u32 char_height)
    : drv_(d)
    , char_w_(char_width)
    , char_h_(char_height)
    , cols_(drv_->screen_width_px() / char_w_)
    , rows_(drv_->screen_height_px() / char_h_)
    , cells_(new Cell[cols_ * rows_]) {
    clear();
}

Terminal::~Terminal() {
    delete[] cells_;
}

void Terminal::set_colour(u32 new_fg, u32 new_bg) {
    fg_ = new_fg;
    bg_ = new_bg;
}

void Terminal::set_cursor(u32 x, u32 y) {
    cx_ = x;
    cy_ = y;
}

void Terminal::put_char(char c) {
    if (c == '\n') {
        new_line();
        return;
    }

    at(cx_, cy_) = {c, fg_, bg_, true};
    advance();
}

void Terminal::put_char_fast(char c) {
    if (c == '\n') {
        new_line();
        return;
    }
    if (c == '\r') {
        cx_ = 0;
        return;
    }

    at(cx_, cy_) = {c, fg_, bg_, true};
    GlyphRun run{&c, 1, cx_ * char_w_, cy_ * char_h_, fg_, bg_};
    drv_->draw_glyph_run(run);
    advance();
}

void Terminal::print(const char* s) {
    while (*s) put_char(*s++);
}

void Terminal::clear() {
    for (u32 i = 0; i < cols_ * rows_; ++i) cells_[i] = {' ', fg_, bg_};

    drv_->fill_rect(0, 0, drv_->screen_width_px(), drv_->screen_height_px(), bg_);
    cx_ = cy_ = 0;
}

void Terminal::clear_char() {
    if (cx_ == 0 && cy_ == 0) return;

    if (cx_ == 0) {
        cy_--;
        cx_ = cols_ - 1;
    } else {
        cx_--;
    }

    put_char_fast(' ');

    if (cx_ == 0) {
        cy_--;
        cx_ = cols_ - 1;
    } else {
        cx_--;
    }
}

void Terminal::new_line() {
    cx_ = 0;
    cy_++;

    if (cy_ >= rows_) {
        scroll();
        cy_ = rows_ - 1;
    }
}

void Terminal::flush() const {
    for (u32 y = 0; y < rows_; ++y) {
        u32 x = 0;
        while (x < cols_) {
            Cell& start = at(x, y);
            if (!start.dirty) {
                x++;
                continue;
            }

            // Finde zusammenhängenden dirty run
            u32 len = 1;
            while (x + len < cols_ && at(x + len, y).dirty && at(x + len, y).fg == start.fg &&
                   at(x + len, y).bg == start.bg) {
                len++;
            }

            draw_run(x, y, &start, len);

            // Dirty-Flags löschen
            for (u32 i = 0; i < len; ++i) at(x + i, y).dirty = false;

            x += len;
        }
    }
}

Terminal::Cell& Terminal::at(u32 x, u32 y) const {
    return cells_[y * cols_ + x];
}

void Terminal::draw_run(u32 cell_x, u32 cell_y, const Cell* run_cells, u32 len) const {
    char buf[256];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;

    for (u32 i = 0; i < len; ++i) buf[i] = run_cells[i].ch;

    buf[len] = '\0';

    GlyphRun run{
        .text = buf,
        .length = len,
        .px = cell_x * char_w_,
        .py = cell_y * char_h_,
        .fg = run_cells[0].fg,
        .bg = run_cells[0].bg
    };

    drv_->draw_glyph_run(run);
}

void Terminal::advance() {
    cx_++;
    if (cx_ >= cols_) new_line();
}

void Terminal::scroll() const {
    drv_->scroll_pixels(char_h_);

    for (u32 y = 1; y < rows_; ++y)
        for (u32 x = 0; x < cols_; ++x) at(x, y - 1) = at(x, y);

    for (u32 x = 0; x < cols_; ++x) at(x, rows_ - 1) = {' ', fg_, bg_};
}