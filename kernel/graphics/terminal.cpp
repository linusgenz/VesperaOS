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

#include <vespera/graphics.h>
#include <vespera/terminal.h>

#include "IRenderDriver.h"
#include "font/glyph_cache.h"
#include "font/psf_glyph_provider.h"

font_t* system_font = nullptr;
Terminal* global_terminal = nullptr;

static u32 blend(const u32 fg, const u32 bg, const u8 alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    u32 rb_fg = fg & 0x00FF00FF;
    u32 g_fg = fg & 0x0000FF00;
    u32 rb_bg = bg & 0x00FF00FF;
    u32 g_bg = bg & 0x0000FF00;

    u32 rb = (rb_fg * alpha + rb_bg * (255 - alpha)) >> 8;
    u32 g = (g_fg * alpha + g_bg * (255 - alpha)) >> 8;

    return (rb & 0x00FF00FF) | (g & 0x0000FF00) | 0xFF000000;
}

Terminal::Terminal(IRenderDriver* d, const u32 char_width, const u32 char_height)
    : drv_(d)
    , glyphs_(new PsfGlyphProvider(system_font))
    , cache_(new GlyphCache())
    , char_w_(char_width)
    , char_h_(char_height)
    , cols_(d->screen_width_px() / char_width)
    , rows_(d->screen_height_px() / char_height)
    , cells_(new Cell[cols_ * rows_]) {
    clear();
}

Terminal::~Terminal() {
    delete[] cells_;
    delete cache_;
    delete glyphs_;
}

void Terminal::set_glyph_provider(IGlyphProvider* provider) {
    if (!provider) return;

    for (usize i = 0; i < cols_ * rows_; i++) cells_[i].dirty = true;

    flush();

    delete glyphs_;
    glyphs_ = provider;

    cache_->invalidate_all();

    const u32 new_char_w = glyphs_->char_width();
    const u32 new_char_h = glyphs_->line_height();
    const usize new_cols = drv_->screen_width_px() / new_char_w;
    const usize new_rows = drv_->screen_height_px() / new_char_h;

    delete[] cells_;
    cells_ = new Cell[new_cols * new_rows];

    cx_ = 0;
    cy_ = 0;

    char_w_ = new_char_w;
    char_h_ = new_char_h;
    cols_ = new_cols;
    rows_ = new_rows;

    clear();
}

void Terminal::set_colour(const u32 new_fg, const u32 new_bg) {
    fg_ = new_fg;
    bg_ = new_bg;
}

void Terminal::set_cursor(const u32 x, const u32 y) {
    cx_ = x;
    cy_ = y;
}

void Terminal::put_char(const char c) {
    if (c == '\n') {
        new_line();
        return;
    }
    at(cx_, cy_) = {static_cast<u32>(static_cast<u8>(c)), fg_, bg_, true};
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
    at(cx_, cy_) = {static_cast<u32>(static_cast<u8>(c)), fg_, bg_, true};
    draw_cell(cx_, cy_);
    advance();
}

void Terminal::print(const char* s) {
    while (*s) put_char(*s++);
}

void Terminal::put_codepoint(const u32 cp)
{
    if (cp == '\n') {
        new_line();
        return;
    }
    if (cp == '\r') {
        cx_ = 0;
        return;
    }

    at(cx_, cy_) = {cp, fg_, bg_, true};
    advance();
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
    for (u32 y = 0; y < rows_; y++) {
        for (u32 x = 0; x < cols_; x++) {
            if (at(x, y).dirty) {
                draw_cell(x, y);
                at(x, y).dirty = false;
            }
        }
    }
}

Terminal::Cell& Terminal::at(const u32 x, const u32 y) const {
    return cells_[y * cols_ + x];
}

void Terminal::draw_cell(const u32 cx, const u32 cy) const {
    const Cell& cell = at(cx, cy);
    const u32 px = cx * char_w_;
    const u32 py = cy * char_h_;

    const GlyphCacheKey key{cell.codepoint, cell.fg, cell.bg};
    if (const GlyphCacheEntry* cached = cache_->find(key)) {
        drv_->blit_buffer(cached->pixels, cached->width, cached->height, px, py);
        return;
    }

    // Rasterize glyph
    const RenderedGlyph* g = glyphs_->get_glyph(cell.codepoint);
    if (!g) {
        drv_->fill_rect(px, py, char_w_, char_h_, cell.bg);
        return;
    }

    const usize bw = char_w_;
    const usize bh = char_h_;
    auto* pixels = static_cast<u32*>(kernel::memory::malloc(bw * bh * sizeof(u32)));

    for (usize i = 0; i < bw * bh; i++) pixels[i] = cell.bg;

    // Glyph-Bitmap mit bearing in die Zelle malen
    const i32 base_y = static_cast<i32>(glyphs_->baseline()) - g->bearing_y;
    const i32 base_x = g->bearing_x;

    for (u32 row = 0; row < g->height; row++) {
        i32 dst_y = base_y + static_cast<i32>(row);
        if (dst_y < 0 || static_cast<u32>(dst_y) >= bh) continue;

        for (u32 col = 0; col < g->width; col++) {
            i32 dst_x = base_x + static_cast<i32>(col);
            if (dst_x < 0 || static_cast<u32>(dst_x) >= bw) continue;

            u8 alpha = g->bitmap[row * g->width + col];
            pixels[dst_y * bw + dst_x] = blend(cell.fg, cell.bg, alpha);
        }
    }

    drv_->blit_buffer(pixels, bw, bh, px, py);

    // Add to cache, cache takes ownership of the pixels
    cache_->insert(key, pixels, bw, bh);
}

void Terminal::advance() {
    cx_++;
    if (cx_ >= cols_) new_line();
}

void Terminal::scroll() const {
    drv_->scroll_pixels(char_h_);
    for (u32 y = 1; y < rows_; y++) {
        for (u32 x = 0; x < cols_; x++) {
            at(x, y - 1) = at(x, y);
        }
    }
    for (u32 x = 0; x < cols_; x++) {
        cells_[(rows_ - 1) * cols_ + x] = {' ', fg_, bg_, false};
    }
}