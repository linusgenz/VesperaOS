// tty.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 09.09.25.
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

#include <basic_renderer.h>
#include "tty.h"

#include <log.h>
#include <scheduling.h>

namespace kernel::tty {

    static TTY tty0;
    TTY* active_tty = &tty0;

    void tty_init() {
        tty0.head = tty0.tail = 0;
        tty0.canonical = true;
    }

    void tty_handle_input(const kernel::input::InputEvent& ev) {
        if (ev.device != kernel::input::InputDeviceType::KEYBOARD) return;
        if (ev.action != kernel::input::KeyAction::PRESS) return;

        char c = ev.ascii;
        if (!c) return;

        if (c == '\b') {
            if (tty0.head != tty0.tail) {
                tty0.head = (tty0.head - 1 + TTY::BUFFER_SIZE) % TTY::BUFFER_SIZE;
                global_renderer->clear_char();
            }
            return;
        }

        if (c == '\n') {
            global_renderer->new_line();
        } else {
            global_renderer->put_char(c);
        }

        size_t next = (tty0.head + 1) % TTY::BUFFER_SIZE;
        if (next != tty0.tail) {
            tty0.buffer[tty0.head] = c;
            tty0.head = next;
        }
    }

    size_t tty_read(char* buf, size_t count) {
        size_t read = 0;
        while (read < count) {
            if (tty0.head == tty0.tail) {
             //   kernel::scheduling::yield();
                continue;
            }
            char c = tty0.buffer[tty0.tail];
            tty0.tail = (tty0.tail + 1) % TTY::BUFFER_SIZE;
            buf[read++] = c;

            if (tty0.canonical && c == '\n') {
                break;
            }
        }

        return read;
    }

}
