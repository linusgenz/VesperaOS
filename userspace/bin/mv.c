// mv.c
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
    puts("Usage: mv <source> <destination>");
    puts("       mv --help");
    puts("");
    puts("Rename or move a file or directory.");
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

    const char* source = argv[1];
    const char* dest = argv[2];

    int64_t result = sys_rename((uint64_t)source, (uint64_t)dest, 0, 0, 0, 0);

    if (result < 0) {
        switch ((int)result) {
            case -ENOENT:
                printf("mv: cannot stat '%s': No such file or directory\n", source);
                break;
            case -EACCES:
                printf("mv: cannot move '%s' to '%s': Permission denied\n", source, dest);
                break;
            case -EROFS:
                printf("mv: cannot move '%s': Read-only filesystem\n", source);
                break;
            case -EEXIST:
                printf("mv: cannot move '%s' to '%s': File exists\n", source, dest);
                break;
            case -ENOTDIR:
                printf("mv: cannot move '%s': Not a directory\n", source);
                break;
            case -ENOTEMPTY:
                printf("mv: cannot move '%s': Directory not empty\n", source);
                break;
            case -EXDEV:
                printf("mv: cannot move '%s' to '%s': Cross-device link\n", source, dest);
                break;
            case -EINVAL:
                printf("mv: invalid argument\n");
                break;
            default:
                printf("mv: cannot move '%s' to '%s': %s\n", source, dest, strerror((int)result));
                break;
        }
        return 1;
    }

    return 0;
}