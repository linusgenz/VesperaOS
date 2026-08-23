// libgen.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.08.26.
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

#include <libgen.h>
#include <string.h>

static char dot[] = ".";

char* dirname(char* path) {
    if (path == NULL || *path == '\0')
        return dot;

    char* last_slash = strrchr(path, '/');
    if (last_slash == NULL)
        return dot;

    if (last_slash == path)
        return "/";

    char* p = last_slash;
    while (p > path && *p == '/')
        p--;
    if (p == path && *path == '/')
        return "/";

    *(p + 1) = '\0';
    return path;
}

char* basename(char* path) {
    if (path == NULL || *path == '\0')
        return dot;

    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }

    if (strcmp(path, "/") == 0)
        return path;

    char* last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}
