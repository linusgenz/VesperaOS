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

#include <cstdint>

#include <graphics.h>
#include "../../kernel/graphics/IRenderDriver.h"


class Terminal
{
    struct Cell
    {
        char ch;
        uint32_t fg;
        uint32_t bg;
        bool dirty;
    };

    IRenderDriver* drv = nullptr;

    uint32_t cols{};
    uint32_t rows{};

    uint32_t char_w{};
    uint32_t char_h{};

    uint32_t cx = 0;
    uint32_t cy = 0;

    uint32_t fg = 0xFFFFFFFF;
    uint32_t bg = 0x00000000;

    Cell* cells{};

public:
    Terminal(IRenderDriver* d, uint32_t char_width, uint32_t char_height);
    ~Terminal();

    void set_colour(uint32_t new_fg, uint32_t new_bg);
    void set_cursor(uint32_t x, uint32_t y);
    void put_char(char c);
    void put_char_fast(char c);
    void print(const char* s);
    void clear();
    void clear_char();
    void new_line();
    void flush();

private:
    Cell& at(uint32_t x, uint32_t y);
    void draw_run(uint32_t cell_x, uint32_t cell_y, const Cell* cells, uint32_t len);
    void advance();
    void scroll();
};

extern FONT* system_font;
extern Terminal* global_terminal;

#endif //VESPERAOS_TERMINAL_H
