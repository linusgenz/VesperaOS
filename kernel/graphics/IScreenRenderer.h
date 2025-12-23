/**
 * @file IScreenRenderer.h
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
#ifndef VESPERAOS_ISCREENRENDERER_H
#define VESPERAOS_ISCREENRENDERER_H

#include <cstdint>

class IScreenRenderer
{
public:
    virtual ~IScreenRenderer() = default;

    virtual void put_char(char c, uint32_t colour, uint32_t bg) = 0;
    virtual void put_char(char c) = 0;
    virtual void print(const char* str, uint32_t colour, uint32_t bg) = 0;
    virtual void print(const char* str) = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual uint32_t get_width() const = 0;
    [[nodiscard]] virtual uint32_t get_height() const = 0;
};

#endif //VESPERAOS_ISCREENRENDERER_H
