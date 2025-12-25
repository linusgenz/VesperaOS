/**
 * @file framebuffer_renderer.h
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

#ifndef VESPERAOS_FRAMEBUFFER_H
#define VESPERAOS_FRAMEBUFFER_H

#include "IScreenRenderer.h"
#include <kernel/basic_renderer.h>

class FramebufferRenderer : public IScreenRenderer
{
    screen_renderer* renderer;

public:
    explicit FramebufferRenderer(screen_renderer* r) : renderer(r)
    {
    }

    void put_char(char c, uint32_t colour, uint32_t bg) override
    {
        renderer->set_colour(colour);
        renderer->set_bg_colour(bg);
        renderer->put_char(c);
    }

    void put_char(char c) override
    {
        renderer->set_colour(0xFFFFFFFF);
        renderer->set_bg_colour(0x00000000);
        renderer->put_char(c);
    }

    void clear_char(uint32_t colour, uint32_t bg) override
    {
        renderer->set_colour(colour);
        renderer->set_bg_colour(bg);
        renderer->clear_char();
    }

    void clear_char() override
    {
        renderer->set_colour(0xFFFFFFFF);
        renderer->set_bg_colour(0x00000000);
        renderer->clear_char();
    }

    void print(const char* str, uint32_t colour, uint32_t bg) override
    {
        renderer->set_colour(colour);
        renderer->set_bg_colour(bg);
        renderer->print(str);
    }

    void print(const char* str) override
    {
        renderer->set_colour(0xFFFFFFFF);
        renderer->set_bg_colour(0x00000000);
        renderer->print(str);
    }

    void clear() override
    {
        renderer->clear();
    }

    void set_cursor(uint32_t x, uint32_t y)
    {
        renderer->set_cursor({x, y});
    }

    [[nodiscard]] uint32_t get_width() const override
    {
        return renderer->TargetFramebuffer->width;
    }

    [[nodiscard]] uint32_t get_height() const override
    {
        return renderer->TargetFramebuffer->height;
    }
};

#endif //VESPERAOS_FRAMEBUFFER_H
