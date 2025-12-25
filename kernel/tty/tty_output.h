/**
 * @file tty_output.h
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
#ifndef VESPERAOS_TTY_OUTPUT_H
#define VESPERAOS_TTY_OUTPUT_H
#include <cstdint>

class TTYOutput {
public:
    virtual ~TTYOutput() = default;

    virtual void put_char(char c) = 0;
    virtual void clear_char() = 0;
    virtual void print(const char* s) = 0;
    virtual void new_line() = 0;

    virtual void clear() = 0;

    virtual void set_fg(uint32_t c) = 0;
    virtual void set_bg(uint32_t c) = 0;

    virtual void set_cursor(uint32_t x, uint32_t y) = 0;
};

#endif //VESPERAOS_TTY_OUTPUT_H