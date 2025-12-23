/**
 * @file gpu_blitter_renderer.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 23.12.25.
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


#ifndef VESPERAOS_GPU_BLITTER_RENDERER_H
#define VESPERAOS_GPU_BLITTER_RENDERER_H

#include "gpu_blt.h"
#include "../../include/graphics.h"
#include "../../kernel/graphics/IScreenRenderer.h"

class GPUBlitterRenderer : public IScreenRenderer
{
    GpuBltDriver* gpu;
    Point cursor_position{0, 0};
    uint32_t char_width;
    uint32_t char_height;

public:
    explicit GPUBlitterRenderer(GpuBltDriver* g, uint32_t font_width, uint32_t font_height) : gpu(g)
    {
        char_width = font_width;
        char_height = font_height;
    }

    void put_char(char c, uint32_t colour, uint32_t bg) override
    {
        if (c == '\n')
        {
            new_line();
            return;
        }

        char buf[2] = {c, '\0'};
        gpu->draw_str(buf, cursor_position.X, cursor_position.Y, colour, bg);
        cursor_position.X += char_width;
    }

    void put_char(char c) override
    {
        put_char(c, 0xFFFFFFFF, 0x00000000);
    }

    void print(const char* str, uint32_t colour, uint32_t bg) override
    {
        if (!str || !*str) return;

        const char* line_start = str;
        uint32_t chars_in_line = 0;

        const uint32_t max_chars_per_line = get_width() / char_width;

        for (const char* p = str; ; ++p)
        {
            char c = *p;

            bool end_of_string = (c == '\0');
            bool newline = (c == '\n');
            bool line_full = (chars_in_line >= max_chars_per_line);

            if (end_of_string || newline || line_full)
            {
                if (chars_in_line > 0)
                {
                    // temporärer Buffer für genau EINEN GPU-Call
                    char buf[256]; // ausreichend für Kernel-Logs
                    if (chars_in_line >= sizeof(buf))
                        chars_in_line = sizeof(buf) - 1;

                    for (uint32_t i = 0; i < chars_in_line; ++i)
                        buf[i] = line_start[i];

                    buf[chars_in_line] = '\0';

                    gpu->draw_str(
                        buf,
                        cursor_position.X,
                        cursor_position.Y,
                        colour,
                        bg
                    );
                }

                cursor_position.X += chars_in_line * char_width;

                if (newline || line_full)
                    new_line();

                if (end_of_string)
                    break;

                line_start = p + 1;
                chars_in_line = 0;
                continue;
            }

            chars_in_line++;
        }
    }


    void print(const char* str) override
    {
        print(str, 0xFFFFFFFF, 0x00000000);
    }

    void clear() override
    {
        BltRect rect = {
            0, 0,
            get_width(),
            get_height(),
        };
        gpu->rect(rect, 0);
        cursor_position = {0, 0};
    }

    [[nodiscard]] uint32_t get_width() const override
    {
        return gpu->get_width();
    }

    [[nodiscard]] uint32_t get_height() const override
    {
        return gpu->get_height();
    }

    void new_line()
    {
        cursor_position.X = 0;
        cursor_position.Y += char_height;

        if (cursor_position.Y + char_height > get_height())
        {
            gpu->scroll(char_height);
            cursor_position.Y = get_height() - char_height;
        }
    }
};

#endif //VESPERAOS_GPU_BLITTER_RENDERER_H