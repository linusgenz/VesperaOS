// keyboard.cpp
//
// LuminOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.07.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#include "keyboard.h"
#include "buffer.h"
#include "qwerty.h"

namespace input::keyboard {

    static bool shift_left = false;
    static bool shift_right = false;

    void init() {
        init_buffer();
        shift_left = shift_right = false;
    }

    void handle_scancode(uint8_t scancode) {
        using namespace qwerty;

        switch (scancode) {
            case LEFT_SHIFT:  shift_left = true; return;
            case RIGHT_SHIFT: shift_right = true; return;
            case LEFT_SHIFT + 0x80:  shift_left = false; return;
            case RIGHT_SHIFT + 0x80: shift_right = false; return;
        }

        bool shift = shift_left || shift_right;

        if (scancode == BACKSPACE) {
            write_char('\b');
            return;
        }
        if (scancode == ENTER) {
            write_char('\n'); // Kommandoende
            return;
        }

        char c = translate(scancode, shift);
        if (c != 0) {
            write_char(c);
        }
    }

    bool read_char(char& c) {
        return buffer_read_char(c);
    }

}
