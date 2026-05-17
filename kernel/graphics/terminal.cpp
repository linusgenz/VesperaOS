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

#include <vespera/graphics/display_manager.h>
#include <vespera/mm/memory.h>
#include <vespera/terminal.h>

#include "font/glyph_cache.h"
#include "font/psf_glyph_provider.h"
#include "scrollback_buffer.h"
#include "vespera/graphics/IRenderDriver.h"

PsfFont* system_font = nullptr;

namespace {

    u32 blend(const u32 fg, const u32 bg, const u8 alpha) {
        if (alpha == 0) return bg;
        if (alpha == 255) return fg;

        const u32 rb_fg = fg & 0x00FF00FFu;
        const u32 g_fg = fg & 0x0000FF00u;
        const u32 rb_bg = bg & 0x00FF00FFu;
        const u32 g_bg = bg & 0x0000FF00u;

        const u32 rb = (rb_fg * alpha + rb_bg * (255u - alpha)) >> 8u;
        const u32 g = (g_fg * alpha + g_bg * (255u - alpha)) >> 8u;

        return (rb & 0x00FF00FFu) | (g & 0x0000FF00u) | 0xFF000000u;
    }

    void backend_hook(const DisplayBackend backend, void* ctx) {
        auto* term = static_cast<Terminal*>(ctx);
        term->on_backend_changed(backend);
    }

}  // anonymous namespace

Terminal::Terminal(IRenderDriver* d, const u32 char_width, const u32 char_height)
    : drv_(d)
    , glyphs_(new PsfGlyphProvider(system_font))
    , cache_(new GlyphCache())
    , char_w_(char_width)
    , char_h_(char_height)
    , cols_(d->screen_width_px() / char_width)
    , rows_(d->screen_height_px() / char_height)
    , sb_(new ScrollbackBuffer(cols_, rows_, 2000))
    , screen_w_(d->screen_width_px())
    , screen_h_(d->screen_height_px())
    , screen_buf_(
          static_cast<u32*>(kernel::memory::malloc(d->screen_width_px() * d->screen_height_px() * sizeof(u32)))
      ) {
    DisplayManager::register_backend_hook(backend_hook, this);
    clear();
}

Terminal::~Terminal() {
    delete sb_;
    delete cache_;
    delete glyphs_;
    kernel::memory::free(screen_buf_);
}

void Terminal::on_backend_changed(const DisplayBackend backend) {
    if (!backend.drv) return;

    drv_ = backend.drv;
    screen_w_ = drv_->screen_width_px();
    screen_h_ = drv_->screen_height_px();
    cols_ = screen_w_ / char_w_;
    rows_ = screen_h_ / char_h_;

    kernel::memory::free(screen_buf_);
    screen_buf_ = static_cast<u32*>(kernel::memory::malloc(screen_w_ * screen_h_ * sizeof(u32)));

    delete sb_;
    sb_ = new ScrollbackBuffer(cols_, rows_, 2000);

    cache_->invalidate_all();

    fill_buf_rect(0, 0, screen_w_, screen_h_, bg_);
    mark_dirty_px(0, 0, screen_w_, screen_h_);
    commit_dirty();
}

void Terminal::fill_buf_rect(const u32 x, const u32 y, const u32 w, const u32 h, const u32 color) {
    const u32 x_end = x + w;
    const u32 y_end = y + h;
    for (u32 row = y; row < y_end; row++) {
        u32* line = screen_buf_ + row * screen_w_ + x;
        for (u32 col = 0, cols = x_end - x; col < cols; col++) line[col] = color;
    }
}

void Terminal::mark_dirty_px(const u32 x, const u32 y, const u32 w, const u32 h) {
    const u32 x1 = x + w;
    const u32 y1 = y + h;
    if (!has_dirty_) {
        dirty_x0_ = x;
        dirty_y0_ = y;
        dirty_x1_ = x1;
        dirty_y1_ = y1;
        has_dirty_ = true;
        return;
    }
    if (x < dirty_x0_) dirty_x0_ = x;
    if (y < dirty_y0_) dirty_y0_ = y;
    if (x1 > dirty_x1_) dirty_x1_ = x1;
    if (y1 > dirty_y1_) dirty_y1_ = y1;
}

void Terminal::commit_dirty() {
    if (!has_dirty_) return;

    drv_->blit_region(
        screen_buf_, screen_w_, dirty_x0_, dirty_y0_, dirty_x1_ - dirty_x0_, dirty_y1_ - dirty_y0_, dirty_x0_, dirty_y0_
    );

    has_dirty_ = false;
}

void Terminal::draw_cell(const u32 cx, const u32 cy) {
    const Cell& cell = sb_->at(cx, cy);
    const u32 px = cx * char_w_;
    const u32 py = cy * char_h_;

    const GlyphCacheKey key{cell.codepoint, cell.fg, cell.bg};
    if (const GlyphCacheEntry* cached = cache_->find(key)) {
        for (u32 row = 0; row < cached->height; row++) {
            const u32* src = cached->pixels + row * cached->width;
            u32* dst = screen_buf_ + static_cast<usize>((py + row) * screen_w_) + px;
            for (u32 col = 0; col < cached->width; col++) dst[col] = src[col];
        }
        mark_dirty_px(px, py, char_w_, char_h_);
        return;
    }

    const RenderedGlyph* g = glyphs_->get_glyph(cell.codepoint);
    if (!g) {
        fill_buf_rect(px, py, char_w_, char_h_, cell.bg);
        mark_dirty_px(px, py, char_w_, char_h_);
        return;
    }

    // Allocate cell pixel buffer, fill background, blend glyph.
    // The cache takes ownership of this allocation.
    auto* pixels = static_cast<u32*>(kernel::memory::malloc(char_w_ * char_h_ * sizeof(u32)));
    for (usize i = 0; i < char_w_ * char_h_; i++) pixels[i] = cell.bg;

    const i32 base_y = static_cast<i32>(glyphs_->baseline()) - g->bearing_y;
    const i32 base_x = g->bearing_x;

    for (u32 row = 0; row < g->height; row++) {
        const i32 dst_y = base_y + static_cast<i32>(row);
        if (dst_y < 0 || static_cast<u32>(dst_y) >= char_h_) continue;

        for (u32 col = 0; col < g->width; col++) {
            const i32 dst_x = base_x + static_cast<i32>(col);
            if (dst_x < 0 || static_cast<u32>(dst_x) >= char_w_) continue;

            const u8 alpha = g->bitmap[row * g->width + col];
            pixels[static_cast<u32>(dst_y) * char_w_ + static_cast<u32>(dst_x)] = blend(cell.fg, cell.bg, alpha);
        }
    }

    for (u32 row = 0; row < char_h_; row++) {
        const u32* src = pixels + static_cast<usize>(row * char_w_);
        u32* dst = screen_buf_ + static_cast<usize>((py + row) * screen_w_) + px;
        for (u32 col = 0; col < char_w_; col++) dst[col] = src[col];
    }

    mark_dirty_px(px, py, char_w_, char_h_);

    // Hand pixel buffer to the cache (cache owns the allocation from here on)
    cache_->insert(key, pixels, char_w_, char_h_);
}

void Terminal::flush() {
    for (usize y = 0; y < rows_; y++) {
        for (usize x = 0; x < cols_; x++) {
            Cell& c = sb_->at(x, y);
            if (c.dirty) {
                draw_cell(static_cast<u32>(x), static_cast<u32>(y));
                c.dirty = false;
            }
        }
    }

    draw_cursor();
    commit_dirty();
}

void Terminal::put_codepoint(const u32 cp) {
    if (cp == '\n') {
        new_line();
        return;
    }
    if (cp == '\r') {
        cursor_col_ = 0;
        return;
    }

    Cell& cell = sb_->write_at(cursor_col_);
    cell = {cp, fg_, bg_, true};
    advance();
}

void Terminal::put_char_fast(const char c) {
    if (c == '\n') {
        new_line();
        return;
    }
    if (c == '\r') {
        erase_cursor_under();
        cursor_col_ = 0;
        draw_cursor();
        commit_dirty();
        return;
    }

    erase_cursor_under();

    Cell& cell = sb_->write_at(cursor_col_);
    cell = {static_cast<u32>(static_cast<u8>(c)), fg_, bg_, true};
    draw_cell(cursor_col_, cursor_row_);
    cell.dirty = false;

    advance();
    cursor_activity();
    draw_cursor();

    // One GPU blit covering whatever we touched (cursor erase + new cell + cursor)
    commit_dirty();
}

void Terminal::put_char(const char c) {
    if (c == '\n') {
        new_line();
        return;
    }
    Cell& cell = sb_->write_at(cursor_col_);
    cell = {static_cast<u32>(static_cast<u8>(c)), fg_, bg_, true};
    advance();
}

void Terminal::print(const char* s) {
    while (*s) put_char(*s++);
}

void Terminal::set_glyph_provider(IGlyphProvider* provider) {
    if (!provider) return;

    sb_->mark_viewport_dirty();
    flush();

    delete glyphs_;
    glyphs_ = provider;
    cache_->invalidate_all();

    char_w_ = glyphs_->char_width();
    char_h_ = glyphs_->line_height();
    cols_ = screen_w_ / char_w_;
    rows_ = screen_h_ / char_h_;

    delete sb_;
    sb_ = new ScrollbackBuffer(cols_, rows_, 2000);

    cursor_col_ = 0;
    cursor_row_ = 0;

    clear();
}

void Terminal::set_colour(const u32 new_fg, const u32 new_bg) {
    fg_ = new_fg;
    bg_ = new_bg;
}

void Terminal::set_cursor(const u32 x, const u32 y) {
    erase_cursor_under();
    cursor_col_ = x;
    cursor_row_ = y;
    sb_->set_write_row(y);
    cursor_activity();
    draw_cursor();
}

void Terminal::clear() {
    sb_->clear(fg_, bg_);
    cursor_col_ = 0;
    cursor_row_ = 0;

    fill_buf_rect(0, 0, screen_w_, screen_h_, bg_);
    mark_dirty_px(0, 0, screen_w_, screen_h_);
    commit_dirty();
}

void Terminal::clear_char() {
    if (cursor_col_ == 0 && cursor_row_ == 0) return;

    erase_cursor_under();

    if (cursor_col_ == 0) {
        sb_->retreat_line();
        cursor_row_ = sb_->write_row();
        cursor_col_ = cols_ - 1;
    } else {
        cursor_col_--;
    }
    sb_->set_write_row(cursor_row_);

    Cell& cell = sb_->write_at(cursor_col_);
    cell = {' ', fg_, bg_, true};
    draw_cell(cursor_col_, cursor_row_);
    cell.dirty = false;

    cursor_activity();
    draw_cursor();
    commit_dirty();
}

void Terminal::new_line() {
    erase_cursor_under();
    cursor_col_ = 0;

    const bool did_hw_scroll = sb_->new_line(fg_, bg_);
    cursor_row_ = sb_->write_row();

    if (did_hw_scroll) {
        const usize keep_rows = screen_h_ - char_h_;
        const usize keep_px = keep_rows * static_cast<usize>(screen_w_);
        const u32* src = screen_buf_ + static_cast<usize>(screen_w_) * char_h_;
        u32* dst = screen_buf_;
        for (usize i = 0; i < keep_px; i++) dst[i] = src[i];

        const u32 bottom_py = static_cast<u32>(rows_ - 1) * char_h_;
        fill_buf_rect(0, bottom_py, screen_w_, char_h_, bg_);

        // Bottom row already cleared by ScrollbackBuffer::new_line(); skip re-compositing.
        for (usize col = 0; col < cols_; col++) sb_->at(col, rows_ - 1).dirty = false;

        // The whole screen changed (content shifted + new blank row).
        mark_dirty_px(0, 0, screen_w_, screen_h_);
    }

    cursor_activity();
    draw_cursor();
    commit_dirty();
}

void Terminal::erase_in_line(const int mode, const u32 col, const u32 row) {
    usize start_col = 0;
    usize end_col = 0;
    switch (mode) {
        case 0:
            start_col = col;
            end_col = cols_;
            break;  // cursor → EOL
        case 1:
            start_col = 0;
            end_col = col + 1;
            break;  // BOL → cursor
        case 2:
            start_col = 0;
            end_col = cols_;
            break;  // entire line
        default:
            return;
    }
    for (usize c = start_col; c < end_col; c++) {
        Cell& cell = sb_->at(c, row);
        cell = {' ', fg_, bg_, true};
    }
}

void Terminal::erase_in_display(const int mode, const u32 col, const u32 row) {
    switch (mode) {
        case 0: {  // cursor → end of screen
            erase_in_line(0, col, row);
            for (usize r = row + 1; r < rows_; r++) erase_in_line(2, 0, r);
            break;
        }
        case 1: {  // start of screen → cursor
            for (usize r = 0; r < row; r++) erase_in_line(2, 0, r);
            erase_in_line(1, col, row);
            break;
        }
        case 2:
            clear();
            break;
        default:
            break;
    }
}

void Terminal::draw_cursor() {
    /*
    if (!cursor_visible_ || !cursor_blink_on_) return;
    if (!sb_->is_at_bottom()) return;

    const u32 px = cursor_col_ * char_w_;
    const u32 py = cursor_row_ * char_h_;

    // Draw cursor bar into screen_buf_ (no driver call; caller must commit_dirty)
    fill_buf_rect(px, py, 2, char_h_, fg_);
    mark_dirty_px(px, py, 2, char_h_);
    */
}

void Terminal::erase_cursor_under() {
    // Re-compositing the cell under the cursor clears it automatically.
    // draw_cell(cursor_col_, cursor_row_);
}

void Terminal::tick_cursor() {
    /*
    if (!cursor_visible_) return;

    if (blink_idle_ticks_ < blink_pause_ticks_) {
        blink_idle_ticks_++;
        draw_cell(cursor_col_, cursor_row_);
        draw_cursor();
        commit_dirty();
        return;
    }

    cursor_blink_on_ = !cursor_blink_on_;
    draw_cell(cursor_col_, cursor_row_);
    if (cursor_blink_on_) draw_cursor();
    commit_dirty();
    */
}

void Terminal::set_cursor_visible(const bool v) {
    /*
    cursor_visible_ = v;
    draw_cell(cursor_col_, cursor_row_);
    commit_dirty();
    */
}

void Terminal::cursor_activity() {
    /*
    blink_idle_ticks_ = 0;
    cursor_blink_on_  = true;
    */
}

void Terminal::scrollback_up(const usize lines) {
    sb_->scroll_up(lines);
    flush();
}

void Terminal::scrollback_down(const usize lines) {
    sb_->scroll_down(lines);
    flush();
}

void Terminal::scrollback_to_bottom() {
    sb_->scroll_to_bottom();
    flush();
}

bool Terminal::is_at_bottom() const {
    return sb_->is_at_bottom();
}

void Terminal::advance() {
    cursor_col_++;
    if (cursor_col_ >= cols_) new_line();
}