// tty.h
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

#ifndef VESPERAOS_TTY_H
#define VESPERAOS_TTY_H

#include "../input/input_event.h"
#include <cstddef>

namespace kernel::tty {

    struct TTY {
        static constexpr size_t BUFFER_SIZE = 1024;
        char buffer[BUFFER_SIZE];
        size_t head = 0;
        size_t tail = 0;
        bool canonical = true;
    };

    extern TTY* active_tty;

    void tty_init();
    void tty_handle_input(const kernel::input::InputEvent& ev);
    size_t tty_read(char* buf, size_t count);

}

#endif //VESPERAOS_TTY_H