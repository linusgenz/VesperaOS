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


#include <kernel/terminal.h>
#include "IRenderDriver.h"
#include <graphics.h>

FONT* system_font = nullptr;
Terminal* global_terminal = nullptr;

Terminal::Terminal(IRenderDriver* d, uint32_t char_width, uint32_t char_height)
    : drv(d), char_w(char_width), char_h(char_height)
{
    cols = drv->screen_width_px() / char_w;
    rows = drv->screen_height_px() / char_h;

    cells = new Cell[cols * rows];
    clear();
}

Terminal::~Terminal()
{
    delete[] cells;
}

void Terminal::set_colour(uint32_t new_fg, uint32_t new_bg)
{
    fg = new_fg;
    bg = new_bg;
}

void Terminal::set_cursor(uint32_t x, uint32_t y)
{
    cx = x;
    cy = y;
}

void Terminal::put_char(char c)
{
    if (c == '\n') { new_line(); return; }

    at(cx, cy) = {c, fg, bg};
    advance();
}

void Terminal::put_char_fast(char c)
{
    if (c == '\n') { new_line(); return; }
    if (c == '\r') { cx = 0; return; }

    at(cx, cy) = {c, fg, bg};
    GlyphRun run{ &c, 1, cx*char_w, cy*char_h, fg, bg };
    drv->draw_glyph_run(run);
    advance();
}

void Terminal::print(const char* s)
{
    while (*s)
        put_char(*s++);
}

void Terminal::clear()
{
    for (uint32_t i = 0; i < cols * rows; ++i)
        cells[i] = {' ', fg, bg};

    drv->fill_rect(0, 0, drv->screen_width_px(), drv->screen_height_px(), bg);
    cx = cy = 0;
}

void Terminal::clear_char()
{
    if (cx == 0 && cy == 0) return;

    if (cx == 0) { cy--; cx = cols - 1; }
    else { cx--; }

    put_char(' ');
}

void Terminal::new_line()
{
    cx = 0;
    cy++;

    if (cy >= rows)
    {
        scroll();
        cy = rows - 1;
    }
}

void Terminal::flush()
{
    for (uint32_t y = 0; y < rows; ++y)
    {
        uint32_t x = 0;
        while (x < cols)
        {
            Cell& start = at(x, y);
            uint32_t len = 1;

            while (x + len < cols)
            {
                Cell& c = at(x + len, y);
                if (c.fg != start.fg || c.bg != start.bg)
                    break;
                len++;
            }

            draw_run(x, y, &start, len);
            x += len;
        }
    }
}

Terminal::Cell& Terminal::at(uint32_t x, uint32_t y)
{
    return cells[y * cols + x];
}

void Terminal::draw_run(uint32_t cell_x, uint32_t cell_y, const Cell* cells, uint32_t len)
{
    char buf[256];
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;

    for (uint32_t i = 0; i < len; ++i)
        buf[i] = cells[i].ch;

    buf[len] = '\0';

    GlyphRun run{
        .text = buf,
        .length = len,
        .px = cell_x * char_w,
        .py = cell_y * char_h,
        .fg = cells[0].fg,
        .bg = cells[0].bg
    };

    drv->draw_glyph_run(run);
}

void Terminal::advance()
{
    cx++;
    if (cx >= cols)
        new_line();
}

void Terminal::scroll()
{
    drv->scroll_pixels(char_h);

    for (uint32_t y = 1; y < rows; ++y)
        for (uint32_t x = 0; x < cols; ++x)
            at(x, y - 1) = at(x, y);

    for (uint32_t x = 0; x < cols; ++x)
        at(x, rows - 1) = {' ', fg, bg};
}