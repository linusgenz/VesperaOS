// ps2_mouse.cpp
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

#include "ps2_mouse.h"
#include "mouse.h"
#include "../../../kernel/cpu/io.h"

namespace ps2::mouse {

    static void wait_write() {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(0x64) & 0x2) == 0) return;
        }
    }

    static void wait_read() {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(0x64) & 1) == 1) return;
        }
    }

    void send_cmd(uint8_t byte) {
        wait_write();
        outb(0x64, 0xD4);
        wait_write();
        outb(0x60, byte);
    }

    uint8_t recv_response() {
        wait_read();
        return inb(0x60);
    }

    void init() {
        outb(0x64, 0xA8); // enabling he auxiliary device - mouse

        wait_write();

        outb(0x64, 0x20);
        wait_read();
        uint8_t status = inb(0x60);
        status |= 0b10;
        wait_write();
        outb(0x64, 0x60);
        wait_write();
        outb(0x60, status); // setting correct bit is the "compaq" status byte https://wiki.osdev.org/PS/2_Mouse

        send_cmd(0xF6);
        recv_response();

        send_cmd(0xF4);
        recv_response();

        // Enable IntelliMouse mode for four-byte packet support (for scroll wheel)
        send_cmd(0xF3); // Set sample rate
        send_cmd(200);  // First part of IntelliMouse activation sequence
        send_cmd(0xF3);
        send_cmd(100);
        send_cmd(0xF3);
        send_cmd(80);
    }

}
