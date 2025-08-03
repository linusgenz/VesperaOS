// sys_close.cpp
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

#include "cstdint"
#include "../FileDescriptor.h"
#include "../../include/errno.h"

namespace syscalls::internal {
    int64_t sys_close(uint64_t fd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
        if (fd >= MAX_FDS) return -EBADF;

        FileDescriptor *desc = kernel::get_fd(fd);
        if (!desc) return -EBADF;

        if (desc->node && desc->node->ops && desc->node->ops->close) {
            desc->node->ops->close(desc->node);
        }

        kernel::free_fd(fd);
        return SUCCESS_CODE;
    }
}