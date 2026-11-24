// eventfd.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 25.09.26.
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

#include <vespera/eventfd.h>
#include <errno.h>

#include "fd_table.h"
#include "unistd.h"
#include "stdint.h"
#include "sysstd.h"

int eventfd(unsigned int initval, int flags) {
    const int64_t ret = sys_eventfd2((uint64_t)initval, (uint64_t)flags, 0, 0, 0, 0);

    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }

    const int fd = fd_table_insert(ret);
    if (fd < 0) {
        sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
        return -1;
    }

    return fd;
}

int eventfd_read(int fd, uint64_t* value) {
    if (!value) {
        errno = EINVAL;
        return -1;
    }

    const ssize_t n = read(fd, value, sizeof(uint64_t));

    if (n != (ssize_t)sizeof(uint64_t)) {
        return -1;
    }

    return 0;
}

int eventfd_write(int fd, uint64_t value) {
    const ssize_t n = write(fd, &value, sizeof(uint64_t));

    if (n != (ssize_t)sizeof(uint64_t)) {
        return -1;
    }

    return 0;
}