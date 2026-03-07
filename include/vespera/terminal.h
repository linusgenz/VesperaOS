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

class Terminal {
    struct Cell {
        char ch;
        u32 fg;
        u32 bg;
        bool dirty;
    };

    IRenderDriver* drv_ = nullptr;

    u32 char_w_{};
    u32 char_h_{};

    usize cols_{};
    usize rows_{};

    u32 cx_ = 0;
    u32 cy_ = 0;

    u32 fg_ = 0xFFFFFFFF;
    u32 bg_ = 0x00000000;

    Cell* cells_{};

   public:
    Terminal(IRenderDriver* d, u32 char_width, u32 char_height);
    ~Terminal();

    void set_colour(u32 new_fg, u32 new_bg);
    void set_cursor(u32 x, u32 y);
    void put_char(char c);
    void put_char_fast(char c);
    void print(const char* s);
    void clear();
    void clear_char();
    void new_line();
    void flush() const;

   private:
    [[nodiscard]] Cell& at(u32 x, u32 y) const;
    void draw_run(u32 cell_x, u32 cell_y, const Cell* run_cells, u32 len) const;
    void advance();
    void scroll() const;
};

extern font_t* system_font;
extern Terminal* global_terminal;

#endif  // VESPERAOS_TERMINAL_H
