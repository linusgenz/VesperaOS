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
#include "../internal/fd_table.h"

int64_t ioctl(int fd, ioctl_request_t request, void *arg) {
    FILE_HANDLE handle = fd_table_get(fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }
    return sys_ioctl((uint64_t)handle, request, (uint64_t)arg, 0, 0, 0);
}