// touch.c
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

static void usage(void) {
    puts("Usage: touch [FILE]...");
    puts("Update the access and modification times of each FILE to the current time.");
    puts("A FILE argument that does not exist is created empty.");
    puts("");
    puts("  --help     display this help and exit");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    int rc = 0;

    for (int i = 1; i < argc; i++) {
        const char* path = argv[i];

        FILE_HANDLE fd = open(path, O_CREAT | O_RDWR);
        if (fd < 0) {
            switch ((int)fd) {
                case -EACCES:
                    printf("touch: cannot touch '%s': Permission denied\n", path);
                    break;
                case -EROFS:
                    printf("touch: cannot touch '%s': Read-only filesystem\n", path);
                    break;
                case -ENOSPC:
                    printf("touch: cannot touch '%s': No space left on device\n", path);
                    break;
                default:
                    printf("touch: cannot touch '%s': %s\n", path, strerror((int)fd));
                    break;
            }
            rc = 1;
        } else {
            close(fd);
        }
    }

    return rc;
}