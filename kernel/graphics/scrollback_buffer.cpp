// scrollback_buffer.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 17.03.26.
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

#include "scrollback_buffer.h"
#include <vespera/mm/memory.h>

ScrollbackBuffer::ScrollbackBuffer(usize cols, usize visible_rows, usize capacity)
    : cols_(cols)
    , rows_(visible_rows)
    , capacity_(capacity)
    , write_line_(0)
    , scroll_offset_(0)
{
    pool_ = static_cast<Cell*>(kernel::memory::malloc(capacity_ * cols_ * sizeof(Cell)));
    for (usize i = 0; i < capacity_ * cols_; i++)
        pool_[i] = Cell{};
}

ScrollbackBuffer::~ScrollbackBuffer() {
    kernel::memory::free(pool_);
}

usize ScrollbackBuffer::viewport_top() const {
    const isize bottom = static_cast<isize>(write_line_);
    const isize top    = bottom - static_cast<isize>(rows_ - 1) - static_cast<isize>(scroll_offset_);

    const isize oldest = static_cast<isize>(write_line_) - static_cast<isize>(capacity_) + 1;
    return static_cast<usize>(top < oldest ? oldest : (top < 0 ? 0 : top));
}

Cell& ScrollbackBuffer::write_at(usize col) const {
    return pool_[ring(write_line_) * cols_ + col];
}

void ScrollbackBuffer::new_line(u32 default_fg, u32 default_bg) {
    if (scroll_offset_ == 0) {
        const bool viewport_scrolls = (write_line_ >= rows_ - 1);
        write_line_++;
        clear_line(write_line_, default_fg, default_bg);

        if (viewport_scrolls) {
            mark_viewport_dirty();
        }
    } else {
        write_line_++;
        clear_line(write_line_, default_fg, default_bg);
        if (scroll_offset_ < capacity_ - rows_)
            scroll_offset_++;
    }
}

Cell& ScrollbackBuffer::at(usize col, usize row) const {
    const usize abs_line = viewport_top() + row;
    return pool_[ring(abs_line) * cols_ + col];
}

void ScrollbackBuffer::scroll_up(usize lines) {
    const usize max_offset = (write_line_ >= rows_) ? write_line_ - rows_ + 1 : 0;
    const usize max_scroll = (max_offset < capacity_ - rows_) ? max_offset : capacity_ - rows_;

    scroll_offset_ += lines;
    if (scroll_offset_ > max_scroll)
        scroll_offset_ = max_scroll;

    mark_viewport_dirty();
}

void ScrollbackBuffer::scroll_down(usize lines) {
    scroll_offset_ = (lines >= scroll_offset_) ? 0 : scroll_offset_ - lines;
    mark_viewport_dirty();
}

void ScrollbackBuffer::scroll_to_bottom() {
    scroll_offset_ = 0;
    mark_viewport_dirty();
}

usize ScrollbackBuffer::write_row() const {
    return (write_line_ < rows_) ? write_line_ : rows_ - 1;
}

void ScrollbackBuffer::set_write_row(usize viewport_row) {
    if (viewport_row >= rows_) viewport_row = rows_ - 1;
    write_line_ = viewport_top() + viewport_row;
}

void ScrollbackBuffer::retreat_line() {
    if (write_line_ == 0) return;
    write_line_--;
}

void ScrollbackBuffer::mark_viewport_dirty() const {
    const usize top = viewport_top();
    for (usize row = 0; row < rows_; row++) {
        Cell* line = &pool_[ring(top + row) * cols_];
        for (usize col = 0; col < cols_; col++)
            line[col].dirty = true;
    }
}

void ScrollbackBuffer::clear(u32 fg, u32 bg) {
    write_line_    = 0;
    scroll_offset_ = 0;
    for (usize i = 0; i < capacity_ * cols_; i++) {
        pool_[i] = Cell{' ', fg, bg, true};
    }
}

void ScrollbackBuffer::clear_line(usize abs_line, u32 fg, u32 bg) const {
    Cell* line = &pool_[ring(abs_line) * cols_];
    for (usize col = 0; col < cols_; col++)
        line[col] = Cell{' ', fg, bg, true};
}