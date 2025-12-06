// exec.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 23.09.25.
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

#include <stdlib.h>
#include <string.h>
#include <file.h>
#include <stdio.h>

static char path_buf[256];

const char *find_executable(const char *name) {
    if (!name || !*name) return NULL;

    if (name[0] == '/') {
        if (file_exists(name)) return name;
        return NULL;
    }

    const char *path = getenv("PATH");
    if (!path) return NULL;

    const char *start = path;
    const char *end;

    while (*start) {
        end = strchr(start, ':');
        size_t dir_len = end ? (size_t) (end - start) : strlen(start);

        if (dir_len + 1 + strlen(name) + 1 > sizeof(path_buf)) {
            return NULL;
        }

        memcpy(path_buf, start, dir_len);
        path_buf[dir_len] = '/';
        strcpy(path_buf + dir_len + 1, name);

        if (file_exists(path_buf)) {
            return path_buf;
        }

        if (!end) break;
        start = end + 1;
    }

    return NULL;
}
