// shm.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.05.26.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sysstd.h>
#include <vespera/handles.h>

#define MAX_ERRNO 4095
#define IS_ERR(value) ((uint64_t)(value) >= (uint64_t)-MAX_ERRNO)
#define GET_ERR(value) ((int)(-(int64_t)(value)))

FILE_HANDLE shm_open(const char* name, int oflag, uint32_t mode) {
    if (name == NULL || name[0] != '/') {
        errno = EINVAL;
        return INVALID_HANDLE;
    }

    int64_t ret = sys_shm_open((uint64_t)name, (uint64_t)oflag, (uint64_t)mode, 0, 0, 0);

    if (IS_ERR(ret)) {
        errno = GET_ERR(ret);
        return INVALID_HANDLE;
    }

    return (FILE_HANDLE)ret;
}

FILE_HANDLE shm_unlink(const char* name) {
    if (name == NULL) {
        errno = EINVAL;
        return INVALID_HANDLE;
    }

    int64_t ret = sys_shm_unlink((uint64_t)name, 0, 0, 0, 0, 0);
    if (IS_ERR(ret)) {
        errno = GET_ERR(ret);
        return INVALID_HANDLE;
    }

    return 0;
}