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

#include "../../../include/vespera/cpu/io.h"
#include "vespera/kernel_utils.h"

typedef struct {
    u32 x;
    u32 y;
} point_t;

namespace input::mouse {
    u8 pointer[] = {
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

    static point_t position = {0, 0};

    point_t get_position() {
        return position;
    }

    void mouse_wait() {
        u64 timeout = 100000;
        while (timeout--) {
            if ((inb(0x64) & 0b10) == 0) {
                return;
            }
        }
    }

    void mouse_wait_input() {
        u64 timeout = 100000;
        while (timeout--) {
            if (inb(0x64) & 0b1) {
                return;
            }
        }
    }

    void mouse_write(const u8 value) {
        mouse_wait();
        outb(0x64, 0xD4);
        mouse_wait();
        outb(0x60, value);
    }

    u8 mouse_read() {
        mouse_wait_input();
        return inb(0x60);
    }

    u8 mouse_cycle = 0;
    u8 mouse_packet[4];
    bool mouse_packet_ready = false;
    point_t mouse_position_old;

    void handle_byte(const u8 data) {
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


        const bool x_negative = mouse_packet[0] & PS2_X_SIGN;
        const bool y_negative = mouse_packet[0] & PS2_Y_SIGN;
        const bool x_overflow = mouse_packet[0] & PS2_X_OVERFLOW;
        const bool y_overflow = mouse_packet[0] & PS2_Y_OVERFLOW;

        // Calculate X movement
        if (!x_negative) {
            position.x += mouse_packet[1];
            if (x_overflow) position.x += 255;
        } else {
            mouse_packet[1] = 256 - mouse_packet[1];
            position.x -= mouse_packet[1];
            if (x_overflow) position.x -= 255;
        }

        // Calculate Y movement
        if (!y_negative) {
            position.y -= mouse_packet[2];
            if (y_overflow) position.y -= 255;
        } else {
            mouse_packet[2] = 256 - mouse_packet[2];
            position.y += mouse_packet[2];
            if (y_overflow) position.y += 255;
        }

        if (position.x < 0) position.x = 0;
        if (position.x > target_framebuffer->width - 1)
            position.x = target_framebuffer->width - 1;

        if (position.y < 0) position.y = 0;
        if (position.y > target_framebuffer->height - 1)
            position.y = target_framebuffer->height - 1;

        if (const i8 wheel_movement = static_cast<i8>(mouse_packet[3]); wheel_movement > 0) {
            //scroll down
        } else if (wheel_movement < 0) {
            //scroll up
        }

       // global_terminal->clear_mouse_cursor(pointer, mouse_position_old);
       // global_terminal->draw_overlay_mouse_cursor(pointer, position, WHITE);

        if (mouse_packet[0] & PS2_LEFT_BUTTON) {
        }
        if (mouse_packet[0] & PS2_MIDDLE_BUTTON) {
        }
        if (mouse_packet[0] & PS2_RIGHT_BUTTON) {
        }

        mouse_packet_ready = false;
        mouse_position_old = position;
    }
}
