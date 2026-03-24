// pwd.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.03.26.
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

#define MAX_PATH 512

static void usage(void) {
    puts("Usage: pwd");
    puts("       pwd --help");
    puts("");
    puts("Print the current working directory.");
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        }
        printf("pwd: invalid option -- '%s'\n", argv[i]);
        return 1;
    }

    char cwd[MAX_PATH];
    int64_t result = getcwd(cwd, sizeof(cwd));

    if (result < 0) {
        switch ((int)result) {
            case -EACCES:
                printf("pwd: cannot read current directory: Permission denied\n");
                break;
            case -ENOENT:
                printf("pwd: current directory has been unlinked\n");
                break;
            default:
                printf("pwd: cannot get current directory: %s\n", strerror((int)result));
                break;
        }
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}