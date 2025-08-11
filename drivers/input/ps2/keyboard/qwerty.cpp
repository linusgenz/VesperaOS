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

namespace input::keyboard::qwerty {

    const char ascii_table[] = {
        0 ,  0 , '1', '2',
       '3', '4', '5', '6',
       '7', '8', '9', '0',
       '-', '=',  0 ,  0 ,
       'q', 'w', 'e', 'r',
       't', 'y', 'u', 'i',
       'o', 'p', '[', ']',
        0 ,  0 , 'a', 's',
       'd', 'f', 'g', 'h',
       'j', 'k', 'l', ';',
       '\'', '`', 0 , '\\',
       'z', 'x', 'c', 'v',
       'b', 'n', 'm', ',',
       '.', '/', 0 , '*',
        0 , ' '
   };

    char translate(uint8_t scancode, bool uppercase) {
        if (scancode > sizeof(ascii_table)) return 0;
        char c = ascii_table[scancode];
        if (uppercase && c >= 'a' && c <= 'z') c -= 32;
        return c;
    }

}
