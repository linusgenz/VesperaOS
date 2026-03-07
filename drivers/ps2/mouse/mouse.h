// mouse.h
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

#ifndef MOUSE_H
#define MOUSE_H

#include <vespera/graphics.h>

namespace input::mouse {
#define PS2_LEFT_BUTTON 0b00000001
#define PS2_MIDDLE_BUTTON 0b00000100
#define PS2_RIGHT_BUTTON 0b00000010

#define PS2_X_SIGN 0b00010000
#define PS2_Y_SIGN 0b00100000
#define PS2_X_OVERFLOW 0b001000000
#define PS2_Y_OVERFLOW 0b100000000

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

    point_t get_position();

    void process_mouse_packet();
}

#endif //MOUSE_H
