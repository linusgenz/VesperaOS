// mount.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 20.03.26.
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
#include <mount.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sysstd.h>

static void usage() {
    puts("Usage:");
    puts("  mount <source> <target> <fstype> [-o options]");
    puts("  mount <target> -o remount[,options]");
    puts("");
    puts("Note: You may only use absolute paths");
    puts("");
    puts("Examples:");
    puts("  mount /dev/sda1 /mnt fat32");
    puts("  mount /dev/nvme0n1p1 /mnt fat32 -o ro,noexec");
    puts("  mount /mnt -o remount,rw");
    puts("  mount /mnt -o remount,ro,noexec");
    puts("");
    puts("Supported options:");
    puts("  -h, --help       Show this help");
    puts("  -o ro            Mount read-only");
    puts("  -o rw            Mount read-write (default)");
    puts("  -o noexec        Disallow execution of binaries");
    puts("  -o exec          Allow execution of binaries (default)");
    puts("  -o noatime       Do not update access timestamps on read");
    puts("  -o atime         Update access timestamps on read (default)");
    puts("  -o remount       Remount with new flags (no unmount needed)");
}

static void parse_options(const char* optstr, uint64_t* flags) {
    char buf[128];
    strncpy(buf, optstr, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, ',');

    while (tok) {
        if (strcmp(tok, "ro") == 0) {
            *flags |= MS_RDONLY;
        } else if (strcmp(tok, "rw") == 0) {
            *flags &= ~MS_RDONLY;
        } else if (strcmp(tok, "noexec") == 0) {
            *flags |= MS_NOEXEC;
        } else if (strcmp(tok, "exec") == 0) {
            *flags &= ~MS_NOEXEC;
        } else if (strcmp(tok, "noatime") == 0) {
            *flags |= MS_NOATIME;
        } else if (strcmp(tok, "atime") == 0) {
            *flags &= ~MS_NOATIME;
        } else if (strcmp(tok, "remount") == 0) {
            *flags |= MS_REMOUNT;
        } else {
            printf("mount: unknown option '%s'\n", tok);
        }

        tok = strtok(NULL, ',');
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        usage();
        return 0;
    }

    if (argc == 3 && strcmp(argv[2], "-o") == 0) {
        printf("mount: -o requires argument\n");
        return 1;
    }

    uint64_t flags = 0;
    const char* source = NULL;
    const char* target = NULL;
    const char* fstype = NULL;
    int opts_start = 4;

    if (argc >= 2 && argv[1][0] == '/') {
        if (argc >= 4 && strcmp(argv[2], "-o") == 0) {
            parse_options(argv[3], &flags);
            if (flags & MS_REMOUNT) {
                target = argv[1];
                const int64_t ret = mount(NULL, target, NULL, flags);
                if (ret < 0) {
                    if (ret == -EINVAL)
                        printf("mount: %s is not mounted\n", target);
                    else
                        printf("mount: remount failed (error=%ld)\n", ret);
                    return 1;
                }
                printf("remounted %s\n", target);
                return 0;
            }
        }
    }

    if (argc < 4) {
        printf("mount: missing arguments\n\n");
        usage();
        return 1;
    }

    source = argv[1];
    target = argv[2];
    fstype = argv[3];

    for (int i = opts_start; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                printf("mount: -o requires argument\n");
                return 1;
            }
            parse_options(argv[i + 1], &flags);
            i++;
        } else {
            printf("mount: unknown argument '%s'\n", argv[i]);
            return 1;
        }
    }

    // remount short form
    if (flags & MS_REMOUNT) {
        const int64_t ret = mount(NULL, target, NULL, flags);
        if (ret < 0) {
            if (ret == -EINVAL)
                printf("mount: %s is not mounted\n", target);
            else
                printf("mount: remount failed (error=%ld)\n", ret);
            return 1;
        }
        printf("remounted %s\n", target);
        return 0;
    }

    const int64_t ret = mount(source, target, fstype, flags);

    if (ret < 0) {
        if (ret == -EBUSY)
            printf("mount: device already mounted\n");
        else if (ret == -EINVAL)
            printf("mount: invalid filesystem\n");
        else if (ret == -ENODEV)
            printf("mount: no such block device\n");
        else
            printf("mount: failed (error=%ld)\n", ret);
        return 1;
    }

    printf("mounted %s -> %s (%s)\n", source, target, fstype);
    return 0;
}