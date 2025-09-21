// console_backend.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 20.09.25.
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

#ifndef VESPERAOS_CONSOLE_BACKEND_H
#define VESPERAOS_CONSOLE_BACKEND_H

#include "../tty/tty.h"
#include "../types/handle.h"
#include <errno.h>
#include "../include/basic_renderer.h"

class ConsoleDevice {
public:
    size_t read(void *buffer, size_t size) {
        return kernel::tty::tty_read(reinterpret_cast<char *>(buffer), size);
    }

    int write(const void *buffer, size_t size) {
        if (!global_renderer) return -EIO;
        global_renderer->print((const char *) buffer, size);
        return (ssize_t) size;
    }
};

#endif //VESPERAOS_CONSOLE_BACKEND_H
