// timerfd.c
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

#include <errno.h>
#include <stdint.h>
#include <sysstd.h>

#include "../internal/fd_table.h"

int timerfd_create(const int clockid, const int flags) {
    const int64_t ret = sys_timerfd_create((uint64_t)clockid, (uint64_t)flags, 0, 0, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    const int fd = fd_table_insert(ret);
    if (fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        return -1;
    }

    return fd;
}

int timerfd_settime(const int fd, const int flags,
                     const struct itimerspec* new_value,
                     struct itimerspec* old_value) {
    if (!new_value) {
        errno = EFAULT;
        return -1;
    }

    const int64_t ret = sys_timerfd_settime(fd, flags, (uint64_t)new_value, (uint64_t)old_value, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return ret;
}