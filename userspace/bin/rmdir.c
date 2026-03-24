// rmdir.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>

static void usage(void) {
    puts("Usage: rmdir [DIRECTORY]...");
    puts("Remove the DIRECTORY(ies), if they are empty.");
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
        int64_t res = sys_rmdir((uint64_t)path, 0, 0, 0, 0, 0);

        if (res < 0) {
            switch ((int)res) {
                case -ENOENT:
                    printf("rmdir: failed to remove '%s': No such file or directory\n", path);
                    break;
                case -ENOTDIR:
                    printf("rmdir: failed to remove '%s': Not a directory\n", path);
                    break;
                case -ENOTEMPTY:
                    printf("rmdir: failed to remove '%s': Directory not empty\n", path);
                    break;
                case -EACCES:
                    printf("rmdir: failed to remove '%s': Permission denied\n", path);
                    break;
                case -EROFS:
                    printf("rmdir: failed to remove '%s': Read-only filesystem\n", path);
                    break;
                default:
                    printf("rmdir: failed to remove '%s': %s\n", path, strerror((int)res));
                    break;
            }
            rc = 1;
        }
    }

    return rc;
}