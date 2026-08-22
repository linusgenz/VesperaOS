// fd_table.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.08.26.
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

#ifndef VESPLIB_INTERNAL_FD_TABLE_H
#define VESPLIB_INTERNAL_FD_TABLE_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Matches _SC_OPEN_MAX's current placeholder value in unistd.c. TODO expose max open handles
#define FD_TABLE_MAX 256

void fd_table_init(void);

int fd_table_insert(FILE_HANDLE handle);

int fd_table_insert_at(int fd, FILE_HANDLE handle);

FILE_HANDLE fd_table_get(int fd);

void fd_table_remove(int fd);

int fd_table_valid(int fd);

#ifdef __cplusplus
}
#endif

#endif  // VESPLIB_INTERNAL_FD_TABLE_H
