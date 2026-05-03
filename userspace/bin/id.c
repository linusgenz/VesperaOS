// id.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 27.04.26.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>

static void usage(void) {
    puts("Usage: id [options]");
    puts("       id --help");
    puts("");
    puts("Print real and effective user and group IDs of the current realm.");
    puts("");
    puts("  -u    Print only the effective user ID");
    puts("  -g    Print only the effective group ID");
    puts("  -r    Combined with -u or -g: print the real ID instead");
}

int main(int argc, char** argv) {
    int only_uid  = 0;
    int only_gid  = 0;
    int use_real  = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "-u") == 0) {
            only_uid = 1;
        } else if (strcmp(argv[i], "-g") == 0) {
            only_gid = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            use_real = 1;
        } else {
            printf("id: unknown option: '%s'\n", argv[i]);
            return 1;
        }
    }

    int64_t uid  = sys_getuid(0, 0, 0, 0, 0, 0);
    int64_t euid = sys_geteuid(0, 0, 0, 0, 0, 0);
    int64_t gid  = sys_getgid(0, 0, 0, 0, 0, 0);
    int64_t egid = sys_getegid(0, 0, 0, 0, 0, 0);

    if (only_uid && only_gid) {
        puts("id: cannot combine -u and -g");
        return 1;
    }

    if (only_uid) {
        printf("%lld\n", use_real ? uid : euid);
        return 0;
    }

    if (only_gid) {
        printf("%lld\n", use_real ? gid : egid);
        return 0;
    }

    /* Default: print everything */
    printf("uid=%lld euid=%lld gid=%lld egid=%lld\n", uid, euid, gid, egid);
    return 0;
}
