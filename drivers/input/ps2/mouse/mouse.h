// mouse.h
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

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include "../../../../include/graphics.h"

namespace input::mouse {
#define PS2LeftButton 0b00000001
#define PS2MiddleButton 0b00000100
#define PS2RightButton 0b00000010

#define PS2XSign 0b00010000
#define PS2YSign 0b00100000
#define PS2XOverflow 0b001000000
#define PS2YOverflow 0b100000000

    extern uint8_t pointer[];

    struct MousePacket {
        int dx;
        int dy;
        int wheel;
        bool left;
        bool right;
        bool middle;
    };

    void init();
    void handle_byte(uint8_t data);
    bool read_packet(MousePacket& out);

    Point get_position();

    void process_mouse_packet();
}

#endif //MOUSE_H
