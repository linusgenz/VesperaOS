// cat.c
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

#define BUFSIZ 8192

static void usage(void) {
    puts("Usage: cat [FILE]...");
    puts("Concatenate FILE(s) to standard output.");
    puts("");
    puts("With no FILE, read from standard input.");
    puts("  --help     display this help and exit");
}

static int cat_file(const char* path) {
    FILE_HANDLE fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (fd == -ENOENT)
            printf("cat: %s: No such file or directory\n", path);
        else if (fd == -EISDIR)
            printf("cat: %s: Is a directory\n", path);
        else
            printf("cat: %s: Cannot open file (error %ld)\n", path, fd);
        return 1;
    }

    char buffer[BUFSIZ];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    if (bytes_read < 0) {
        printf("cat: %s: Error reading file\n", path);
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int cat_stdin(void) {
    char buffer[BUFSIZ];
    ssize_t bytes_read;

    while ((bytes_read = read(stdin, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        return cat_stdin();
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (cat_file(argv[i]) != 0) {
            rc = 1;
        }
    }

    return rc;
}