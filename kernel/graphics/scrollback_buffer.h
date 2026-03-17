// scrollback_buffer.h
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
#ifndef VESPERAOS_SCROLLBACK_BUFFER_H
#define VESPERAOS_SCROLLBACK_BUFFER_H

#include <vespera/types.h>

struct Cell {
    u32  codepoint = ' ';
    u32  fg        = 0xFFFFFFFF;
    u32  bg        = 0xFF000000;
    bool dirty     = true;
};

class ScrollbackBuffer {
public:
    /// @param cols         Terminal width
    /// @param visible_rows Number of visible rows (viewport height)
    /// @param capacity     Maximum number of rows in the ring (including scrollback)
    ScrollbackBuffer(usize cols, usize visible_rows, usize capacity = 2000);
    ~ScrollbackBuffer();

    ScrollbackBuffer(const ScrollbackBuffer&)            = delete;
    ScrollbackBuffer& operator=(const ScrollbackBuffer&) = delete;

    /// Cell in the current row.
    [[nodiscard]] Cell& write_at(usize col) const;

    void new_line(u32 default_fg, u32 default_bg);

    void mark_viewport_dirty() const;

    void clear(u32 fg, u32 bg);

    /// Viewport-relative position. (0,0) = top-left.
    [[nodiscard]] Cell& at(usize col, usize row) const;

    void scroll_up(usize lines);     ///< Viewport: orientation for older output
    void scroll_down(usize lines);   ///< Viewport orientation for newer output
    void scroll_to_bottom();         ///< Snap to tail, re-enable auto-follow
    [[nodiscard]] bool is_at_bottom() const        { return scroll_offset_ == 0; }

   // set_write_row()

    [[nodiscard]] usize cols()         const { return cols_; }
    [[nodiscard]] usize visible_rows() const { return rows_; }

    /// The line in the viewport that is currently being written to (only valid if is_at_bottom()).
    [[nodiscard]] usize write_row() const;
    void set_write_row(usize viewport_row);
    void retreat_line();

   private:
    Cell*  pool_;
    usize  cols_;
    usize  rows_;
    usize  capacity_;       ///< Depth of the ring buffer in lines

    usize  write_line_;     ///< Absolute line index of the current write line
    usize  scroll_offset_;  ///< Zeilen über dem Tail (0 = folgt Output)

    [[nodiscard]] usize ring(usize abs) const { return abs % capacity_; }
    [[nodiscard]] usize viewport_top()  const;
    void  clear_line(usize abs_line, u32 fg, u32 bg) const;
};

#endif  // VESPERAOS_SCROLLBACK_BUFFER_H
