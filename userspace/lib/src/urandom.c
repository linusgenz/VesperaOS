// urandom.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 26.09.25.
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

#include "urandom.h"
#include <string.h>
#include <fflags.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define UDEV_PATH "/dev/urandom"

ssize_t getrandom(void *buf, size_t buflen) {
    if (!buf || buflen == 0) {
        return -EINVAL;
    }

    FILE_HANDLE fd = fopen(UDEV_PATH, O_RDONLY);
    if (fd < 0) {
        return -ENOENT;
    }

    ssize_t total = 0;
    uint8_t *out = (uint8_t*)buf;

    // to tolarate partial reads fread in a loop
    while ((size_t)total < buflen) {
        ssize_t n = fread(fd, out + total, buflen - (size_t)total);
        if (n < 0) {
            fclose(fd);
            return -EIO;
        }
        if (n == 0) break; // EOF
        total += n;
    }

    fclose(fd);
    return total;
}

int32_t urandom_u32(void) {
    uint32_t v = 0;
    const ssize_t r = getrandom(&v, sizeof(v));
    if (r != sizeof(v)) {
        return r;
    }
    return v;
}

int64_t urandom_u64(void) {
    uint64_t v = 0;
    const ssize_t r = getrandom(&v, sizeof(v));
    if (r != sizeof(v)) {
        return r;
    }
    return v;
}
