// mkdir.c
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
#include <sysstd.h>

static void usage(void) {
    puts("Usage: mkdir [DIRECTORY]...");
    puts("Create the DIRECTORY(ies), if they do not already exist.");
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
        int64_t res = create(path, C_DIR);

        if (res < 0) {
            switch ((int)res) {
                case -EEXIST:
                    printf("mkdir: cannot create directory '%s': File exists\n", path);
                    break;
                case -ENOENT:
                    printf("mkdir: cannot create directory '%s': No such file or directory\n", path);
                    break;
                case -EACCES:
                    printf("mkdir: cannot create directory '%s': Permission denied\n", path);
                    break;
                case -EROFS:
                    printf("mkdir: cannot create directory '%s': Read-only filesystem\n", path);
                    break;
                case -ENOSPC:
                    printf("mkdir: cannot create directory '%s': No space left on device\n", path);
                    break;
                default:
                    printf("mkdir: cannot create directory '%s': %s\n", path, strerror((int)res));
                    break;
            }
            rc = 1;
        }
    }

    return rc;
}