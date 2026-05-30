// serial.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.05.26.
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

#include <vespera/cpu/io.h>

namespace serial {
    static constexpr u16 COM1 = 0x3F8;

    inline bool tx_empty() {
        return inb(COM1 + 5) & 0x20;
    }

    inline void write_char(char c) {
        while (!tx_empty()) {
            asm volatile("pause");
        }

        outb(COM1, static_cast<u8>(c));
    }

    void write(const void* buf, usize count) {
        const char* s = static_cast<const char*>(buf);

        for (usize i = 0; i < count; ++i) {
            char c = s[i];

            if (c == '\n') write_char('\r');

            write_char(c);
        }
    }

}