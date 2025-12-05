// qwerty.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 30.07.25.
//
// This file is part of VesperaOS.
// 
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "qwerty.h"

namespace ps2::keyboard::qwerty {

    struct ps2_keymap_entry {
        uint8_t scancode;
        char normal;
        char shifted;
    };

    static constexpr ps2_keymap_entry PS2_KEYMAP[] = {
        {0x02, '1', '!'}, {0x03, '2', '@'}, {0x04, '3', '#'}, {0x05, '4', '$'},
        {0x06, '5', '%'}, {0x07, '6', '^'}, {0x08, '7', '&'}, {0x09, '8', '*'},
        {0x0A, '9', '('}, {0x0B, '0', ')'},
        {0x0C, '-', '_'}, {0x0D, '=', '+'}, {0x1A, '[', '{'}, {0x1B, ']', '}'},
        {0x2B, '\\', '|'}, {0x27, ';', ':'}, {0x28, '\'', '"'}, {0x29, '`', '~'},
        {0x33, ',', '<'}, {0x34, '.', '>'}, {0x35, '/', '?'}, {0x39, ' ', ' '},

        // letters
        {0x10, 'q', 'Q'}, {0x11, 'w', 'W'}, {0x12, 'e', 'E'}, {0x13, 'r', 'R'},
        {0x14, 't', 'T'}, {0x15, 'y', 'Y'}, {0x16, 'u', 'U'}, {0x17, 'i', 'I'},
        {0x18, 'o', 'O'}, {0x19, 'p', 'P'}, {0x1E, 'a', 'A'}, {0x1F, 's', 'S'},
        {0x20, 'd', 'D'}, {0x21, 'f', 'F'}, {0x22, 'g', 'G'}, {0x23, 'h', 'H'},
        {0x24, 'j', 'J'}, {0x25, 'k', 'K'}, {0x26, 'l', 'L'}, {0x2C, 'z', 'Z'},
        {0x2D, 'x', 'X'}, {0x2E, 'c', 'C'}, {0x2F, 'v', 'V'}, {0x30, 'b', 'B'},
        {0x31, 'n', 'N'}, {0x32, 'm', 'M'},
    };

    char translate(uint8_t scancode, bool shift) {
        for (const auto& entry : PS2_KEYMAP) {
            if (entry.scancode == scancode) {
                return shift ? entry.shifted : entry.normal;
            }
        }
        return 0;
    }

}
