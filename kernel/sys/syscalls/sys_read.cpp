// sys_read.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 02.08.25.
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

#include "../FileDescriptor.h"
#include "cstdint"
#include "../../../drivers/input/ps2/keyboard/buffer.h"
#include "../../../drivers/input/ps2/keyboard/keyboard.h"
#include "../../../include/log.h"
#include "../../include/errno.h"

namespace syscalls::internal {
    int64_t sys_read(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t, uint64_t, uint64_t) {
        int64_t fd = static_cast<int>(arg0);
        void* buf = reinterpret_cast<void*>(arg1);
        size_t count = static_cast<size_t>(arg2);

        if (fd < 0 || fd >= MAX_FDS) return -EBADF;
        if (!buf || count == 0) return -EINVAL;

        if (fd == 0) {
            // stdin (Keyboard)
            size_t read = 0;
            char* out = reinterpret_cast<char*>(buf);

            while (read < count) {
                char c;
                if (!input::keyboard::read_char(c)) {
                    // TODO yield pause etc
                    continue;
                }

                if (c == '\b') {
                    global_renderer->clear_char();
                } else if (c != '\n') {
                    global_renderer->put_char(c);
                }

                out[read++] = c;

                if (c == '\n') {
                    global_renderer->new_line();
                    break;
                };
            }

            return read;
        }

        FileDescriptor *desc = kernel::get_fd(fd);
        if (!desc || !desc->node || !desc->node->ops || !desc->node->ops->read) return -EBADF;

        size_t bytes = desc->node->ops->read(desc->node, desc->offset, count, buf);
        desc->offset += bytes;

        return bytes;
    }
}