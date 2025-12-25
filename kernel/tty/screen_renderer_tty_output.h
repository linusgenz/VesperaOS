/**
 * @file screen_renderer_tty_output.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 24.12.25.
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
#ifndef VESPERAOS_SCREEN_RENDERER_TTY_OUTPUT_H
#define VESPERAOS_SCREEN_RENDERER_TTY_OUTPUT_H
#include <cstdint>

#include "tty_output.h"
#include "../graphics/IScreenRenderer.h"

class ScreenRendererTTYOutput final : public TTYOutput
{
    IScreenRenderer* r;
    uint32_t fg = 0xFFFFFFFF;
    uint32_t bg = 0x00000000;

public:
    explicit ScreenRendererTTYOutput(IScreenRenderer* renderer)
        : r(renderer)
    {
    }

    void put_char(char c) override
    {
        r->put_char(c, fg, bg);
    }

    void clear_char() override
    {
        r->clear_char();
    }

    void print(const char* s) override
    {
        r->print(s);
    }

    void new_line() override
    {
        r->put_char('\n', fg, bg);
    }

    void clear() override
    {
        r->clear();
    }

    void set_fg(uint32_t c) override
    {
        fg = c;
    }

    void set_bg(uint32_t c) override
    {
        bg = c;
    }

    void set_cursor(uint32_t x, uint32_t y) override
    {
        r->set_cursor(x, y);
    }
};


#endif //VESPERAOS_SCREEN_RENDERER_TTY_OUTPUT_H
