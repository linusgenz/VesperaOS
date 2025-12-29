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

#include "IRenderDriver.h"

struct Cell
{
    char ch;
    uint32_t fg;
    uint32_t bg;
};

class Terminal
{
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
    Terminal(
        IRenderDriver* d,
        uint32_t char_width,
        uint32_t char_height
    )
        : drv(d),
          char_w(char_width),
          char_h(char_height)
    {
        cols = drv->screen_width_px() / char_w;
        rows = drv->screen_height_px() / char_h;

        cells = new Cell[cols * rows];
        clear();
    }

    ~Terminal()
    {
        delete[] cells;
    }

    void set_colour(uint32_t new_fg, uint32_t new_bg)
    {
        fg = new_fg;
        bg = new_bg;
    }

    void set_cursor(uint32_t x, uint32_t y)
    {
        cx = x;
        cy = y;
    }

    void put_char(char c)
    {
        if (c == '\n')
        {
            new_line();
            return;
        }

        at(cx, cy) = {c, fg, bg};
        advance();
    }

    void print(const char* s)
    {
        while (*s)
            put_char(*s++);
    }

    void clear()
    {
        for (uint32_t i = 0; i < cols * rows; ++i)
            cells[i] = {' ', fg, bg};

        drv->fill_rect(
            0,
            0,
            drv->screen_width_px(),
            drv->screen_height_px(),
            bg
        );

        cx = cy = 0;
    }

    void clear_char()
    {
        if (cx == 0 && cy == 0)
            return;

        if (cx == 0)
        {
            cy--;
            cx = rows - 1;
        }
        else
        {
            cx--;
        }

        put_char(' ');
    }


    void new_line()
    {
        cx = 0;
        cy++;

        if (cy >= rows)
        {
            scroll();
            cy = rows - 1;
        }
    }

    void flush()
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

private:
    Cell& at(uint32_t x, uint32_t y)
    {
        return cells[y * cols + x];
    }

    void draw_run(
        uint32_t cell_x,
        uint32_t cell_y,
        const Cell* cells,
        uint32_t len
    )
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

    void advance()
    {
        cx++;
        if (cx >= cols)
            new_line();
    }

    void scroll()
    {
        drv->scroll_pixels(char_h);

        // cell buffer scroll
        for (uint32_t y = 1; y < rows; ++y)
        {
            for (uint32_t x = 0; x < cols; ++x)
                at(x, y - 1) = at(x, y);
        }

        for (uint32_t x = 0; x < cols; ++x)
            at(x, rows - 1) = {' ', fg, bg};
    }
};


#endif //VESPERAOS_TERMINAL_H
