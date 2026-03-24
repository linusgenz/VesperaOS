// cp.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.03.26.
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
#include <fflags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 4096

static void usage(void) {
    puts("Usage: cp <SOURCE> <DEST>");
    puts("Copy SOURCE to DEST.");
    puts("");
    puts("  --help     display this help and exit");
}

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    const char* src_path = argv[1];
    const char* dst_path = argv[2];

    FILE_HANDLE src = open(src_path, O_RDONLY);
    if (src < 0) {
        if (src == -ENOENT)
            printf("cp: cannot stat '%s': No such file or directory\n", src_path);
        else if (src == -EACCES)
            printf("cp: cannot open '%s': Permission denied\n", src_path);
        else
            printf("cp: cannot open '%s': %s\n", src_path, strerror((int)src));
        return 1;
    }

    FILE_HANDLE dst = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC);
    if (dst < 0) {
        if (dst == -EACCES)
            printf("cp: cannot create '%s': Permission denied\n", dst_path);
        else if (dst == -EROFS)
            printf("cp: cannot create '%s': Read-only filesystem\n", dst_path);
        else
            printf("cp: cannot create '%s': %s\n", dst_path, strerror((int)dst));
        close(src);
        return 1;
    }

    char buffer[BUFSIZE];
    ssize_t bytes;

    while ((bytes = read(src, buffer, sizeof(buffer))) > 0) {
        ssize_t written = write(dst, buffer, bytes);
        if (written != bytes) {
            printf("cp: write error: %s\n", strerror((int)written));
            close(src);
            close(dst);
            return 1;
        }
    }

    if (bytes < 0) {
        printf("cp: read error: %s\n", strerror((int)bytes));
        close(src);
        close(dst);
        return 1;
    }

    close(src);
    close(dst);
    return 0;
}