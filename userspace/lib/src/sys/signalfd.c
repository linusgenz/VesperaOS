// signalfd.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.09.26.
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

#include <sys/signalfd.h>
#include <errno.h>

#include "../internal/fd_table.h"
#include "stdint.h"
#include "sysstd.h"
#include "vespera/handles.h"

int signalfd(const int fd, const sigset_t* mask, const int flags) {
    if (!mask) {
        errno = EFAULT;
        return -1;
    }

    uint64_t kernel_ufd;
    if (fd < 0) {
        kernel_ufd = (uint64_t)(int64_t)-1;
    } else {
        const FILE_HANDLE handle = fd_table_get(fd);
        if (handle == INVALID_HANDLE) {
            errno = EBADH;
            return -1;
        }
        kernel_ufd = (uint64_t)handle;
    }

    const int64_t ret = sys_signalfd4(
        kernel_ufd,
        (uint64_t)(uintptr_t)mask,
        (uint64_t)sizeof(sigset_t),
        (uint64_t)flags, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    if (fd >= 0) {
        return fd;
    }

    const int new_fd = fd_table_insert(ret);
    if (new_fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        return -1;
    }

    return new_fd;
}
