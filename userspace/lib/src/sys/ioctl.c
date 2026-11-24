// ioctl.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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

#include <sysstd.h>
#include <sys/ioctl.h>
#include <vespera/handles.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include "../internal/fd_table.h"
#include "vespera/dev/drm.h"
#include "vespera/dev/lucifer_drm.h"

int ioctl(int fd, ioctl_request_t request, void *arg) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_ioctl((uint64_t)handle, (uint64_t)request, (uint64_t)arg, 0, 0, 0);

    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    if (request == DRM_IOCTL_PRIME_HANDLE_TO_FD) {
        struct drm_prime_handle* prime = arg;
        // for DRM_IOCTL_PRIME_HANDLE_TO_FD the return value is the file handle which we have to translate into an fd for userspace
        FILE_HANDLE new_handle = prime->fd;
        int new_fd = fd_table_insert(new_handle);
        if (new_fd < 0) {
            sys_close((uint64_t)new_handle, 0, 0, 0, 0, 0);
            return -1;
        }
        prime->fd = new_fd;
    }

    return (int)ret;
}