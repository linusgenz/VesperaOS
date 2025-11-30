// buffer.cpp
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

#include "buffer.h"
#include "keyboard.h"
#include "../../../include/log.h"

namespace input::keyboard {

    static char buffer[BUFFER_SIZE];
    static int head = 0;
    static int tail = 0;

    void init_buffer() {
        head = 0;
        tail = 0;
    }

    bool buffer_read_char(char& c) {
        if (head == tail) return false;
        c = buffer[tail];
        tail = (tail + 1) % BUFFER_SIZE;
        return true;
    }

    void write_char(const char c) {
        int next = (head + 1) % BUFFER_SIZE;
        if (next != tail) {
            buffer[head] = c;
            head = next;
        }
    }

    bool is_empty() {
        return head == tail;
    }

}
