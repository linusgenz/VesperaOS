// stat.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.08.26.
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

#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

int stat(const char *restrict path, struct stat *restrict buf) {
    if (!path || !buf) {
        errno = EINVAL;
        return -1;
    }

    int64_t res = sys_stat((uint64_t)path, (uint64_t)buf, 0, 0, 0, 0);
    if (res < 0) {
        errno = (int)(-res);
        return -1;
    }
    return 0;
}

int fstat(int fd, struct stat *buf) {
    if (fd < 0 || !buf) {
        errno = EINVAL;
        return -1;
    }

    int64_t res = sys_fstat((uint64_t)fd, (uint64_t)buf, 0, 0, 0, 0);
    if (res < 0) {
        errno = (int)(-res);
        return -1;
    }
    return 0;
}