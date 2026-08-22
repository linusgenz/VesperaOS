// fcntl.c
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 14.08.26.
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

#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <sysstd.h>
#include <vespera/handles.h>

#include "internal/fd_table.h"

int open(const char* path, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t)va_arg(args, int);
        va_end(args);
    }

    int64_t ret = sys_open((uint64_t)(uintptr_t)path,
                           (uint64_t)flags,
                           (uint64_t)mode,
                           0, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    FILE_HANDLE handle = (FILE_HANDLE)ret;
    int fd = fd_table_insert(handle);
    if (fd < 0) {
        sys_close((uint64_t)handle, 0, 0, 0, 0, 0);
        return -1;
    }
    return fd;
}

int creat(const char* path) {
    return open(path, O_CREAT | O_WRONLY | O_TRUNC);
}

int fcntl(int fd, int cmd, ...) {
    switch (cmd) {
        case F_DUPFD: {
            // min_fd is consumed for ABI correctness but currently
            // ignored — see header note on F_DUPFD.
            va_list args;
            va_start(args, cmd);
            (void)va_arg(args, int);
            va_end(args);

            FILE_HANDLE handle = fd_table_get(fd);
            if (handle == INVALID_HANDLE) {
                errno = EBADH;
                return -1;
            }

            int64_t ret = sys_dup((uint64_t)handle, 0, 0, 0, 0, 0);
            if (ret < 0) {
                errno = (int)(-ret);
                return -1;
            }

            int new_fd = fd_table_insert((FILE_HANDLE)ret);
            if (new_fd < 0) {
                sys_close((uint64_t)ret, 0, 0, 0, 0, 0);
                return -1;
            }
            return new_fd;
        }

        case F_GETFD:
            // No handle-flag tracking (e.g. close-on-exec) exists yet.
            if (!fd_table_valid(fd)) {
                errno = EBADH;
                return -1;
            }
            return 0;

        case F_SETFD: {
            // Accepted no-op; consume the flags argument for ABI
            // correctness.
            va_list args;
            va_start(args, cmd);
            (void)va_arg(args, int);
            va_end(args);

            if (!fd_table_valid(fd)) {
                errno = EBADH;
                return -1;
            }
            return 0;
        }

        case F_GETFL:
        case F_SETFL:
        case F_GETLK:
        case F_SETLK:
        case F_SETLKW:
            // Not backed by any current syscall. See header note.
            if (!fd_table_valid(fd)) {
                errno = EBADH;
                return -1;
            }
            errno = ENOSYS;
            return -1;

        default:
            errno = EINVAL;
            return -1;
    }
}

int openat(int dirfd, const char* pathname, int flags, ...) {
    mode_t mode = 0;

    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    int64_t ret = sys_openat((uint64_t)dirfd, (uint64_t)pathname, (uint64_t)flags, (uint64_t)mode, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    return (int)ret;
}
