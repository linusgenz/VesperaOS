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

#include <fcntl.h>

#include "vespera/handles.h"
#include "sys/types.h"
#include "../internal/fd_table.h"

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
    if (!buf) {
        errno = EINVAL;
        return -1;
    }

    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t res = sys_fstat((uint64_t)handle, (uint64_t)buf, 0, 0, 0, 0);
    if (res < 0) {
        errno = (int)(-res);
        return -1;
    }
    return 0;
}


int mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    uint64_t native_dirfd = 0;
    if (dirfd == AT_FDCWD || (pathname[0] == '/')) {
        native_dirfd = (uint64_t)(int64_t)AT_FDCWD;
    } else {
        FILE_HANDLE handle = fd_table_get(dirfd);
        if (handle == INVALID_HANDLE) {
            errno = EBADH;
            return -1;
        }
        native_dirfd = (uint64_t)handle;
    }

    int64_t ret = sys_mknodat(native_dirfd, (uint64_t)pathname, (uint64_t)mode, (uint64_t)dev, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int mknod(const char *pathname, mode_t mode, dev_t dev) {
    return mknodat(AT_FDCWD, pathname, mode, dev);
}

int mkfifoat(int dirfd, const char *pathname, mode_t mode) {
    return mknodat(dirfd, pathname, mode | S_IFIFO, 0);
}

int mkfifo(const char *pathname, mode_t mode) {
    return mkfifoat(AT_FDCWD, pathname, mode);
}

int mkdir(const char* path, mode_t mode) {
    long ret = (int)sys_mkdir((uint64_t)path, (uint64_t)mode,0,0,0,0);
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return 0;
}

int is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.v_node_type == VSTAT_TYPE_DIR;
}

int is_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.v_node_type == VSTAT_TYPE_FILE;
}