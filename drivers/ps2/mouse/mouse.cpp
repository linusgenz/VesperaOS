// mouse.cpp
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

#include "mouse.h"
#include "../../../kernel/cpu/io.h"
#include <kernel/basic_renderer.h>

namespace input::mouse {
    uint8_t pointer[] = {
        0b10000000, 0b00000000,
        0b11000000, 0b00000000,
        0b11100000, 0b00000000,
        0b11110000, 0b00000000,
        0b11111000, 0b00000000,
        0b11111100, 0b00000000,
        0b11111110, 0b00000000,
        0b11111111, 0b00000000,
        0b11111111, 0b10000000,
        0b11111111, 0b11000000,
        0b11111111, 0b11100000,
        0b11111111, 0b10000000,
        0b11111111, 0b00000000,
        0b11000111, 0b00000000,
        0b00000011, 0b00000000,
        0b00000001, 0b00000000,
    };
    
    static Point position = {0, 0};

    Point get_position() {
        return position;
    }

    void mouse_wait() {
        uint64_t timeout = 100000;
        while (timeout--) {
            if ((inb(0x64) & 0b10) == 0) {
                return;
            }
        }
    }

    void mouse_wait_input() {
        uint64_t timeout = 100000;
        while (timeout--) {
            if (inb(0x64) & 0b1) {
                return;
            }
        }
    }

    void mouse_write(uint8_t value) {
        mouse_wait();
        outb(0x64, 0xD4);
        mouse_wait();
        outb(0x60, value);
    }

    uint8_t mouse_read() {
        mouse_wait_input();
        return inb(0x60);
    }

    uint8_t mouse_cycle = 0;
    uint8_t mouse_packet[4];
    bool mouse_packet_ready = false;
    Point mouse_position_old;

    void handle_byte(uint8_t data) {
        static bool skip = true;
        if (skip) {
            skip = false;
            return;
        }

        switch (mouse_cycle) {
            case 0:
                if ((data & 0b00001000) == 0) break;
                mouse_packet[0] = data;
                mouse_cycle++;
                break;
            case 1:
                mouse_packet[1] = data;
                mouse_cycle++;
                break;
            case 2:
                mouse_packet[2] = data;
                mouse_cycle++;
                break;
            case 3:
                mouse_packet[3] = data;
                mouse_packet_ready = true;
                mouse_cycle = 0;
                break;
            default: ;
        }

        if (mouse_packet_ready) {
            process_mouse_packet();
            mouse_packet_ready = false;
        }
    }

    void process_mouse_packet() {
        if (!mouse_packet_ready) return;
        mouse_packet_ready = false;


        bool x_negative = mouse_packet[0] & PS2XSign;
        bool y_negative = mouse_packet[0] & PS2YSign;
        bool x_overflow = mouse_packet[0] & PS2XOverflow;
        bool y_overflow = mouse_packet[0] & PS2YOverflow;

        // Calculate X movement
        if (!x_negative) {
            position.X += mouse_packet[1];
            if (x_overflow) position.X += 255;
        } else {
            mouse_packet[1] = 256 - mouse_packet[1];
            position.X -= mouse_packet[1];
            if (x_overflow) position.X -= 255;
        }

        // Calculate Y movement
        if (!y_negative) {
            position.Y -= mouse_packet[2];
            if (y_overflow) position.Y -= 255;
        } else {
            mouse_packet[2] = 256 - mouse_packet[2];
            position.Y += mouse_packet[2];
            if (y_overflow) position.Y += 255;
        }

        if (position.X < 0) position.X = 0;
        if (position.X > global_renderer->TargetFramebuffer->width - 1)
            position.X = global_renderer->TargetFramebuffer->width - 1;

        if (position.Y < 0) position.Y = 0;
        if (position.Y > global_renderer->TargetFramebuffer->height - 1)
            position.Y = global_renderer->TargetFramebuffer->height - 1;

        int8_t wheel_movement = static_cast<int8_t>(mouse_packet[3]);
        if (wheel_movement > 0) {
            //scroll down
        } else if (wheel_movement < 0) {
            //scroll up
        }

        global_renderer->clear_mouse_cursor(pointer, mouse_position_old);
        global_renderer->draw_overlay_mouse_cursor(pointer, position, WHITE);

        if (mouse_packet[0] & PS2LeftButton) {
        }
        if (mouse_packet[0] & PS2MiddleButton) {
        }
        if (mouse_packet[0] & PS2RightButton) {
        }

        mouse_packet_ready = false;
        mouse_position_old = position;
    }
}
