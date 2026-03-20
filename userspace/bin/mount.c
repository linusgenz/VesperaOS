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

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sysstd.h>
#include <errno.h>

static void usage() {
    puts("Usage:");
    puts("  mount <source> <target> <fstype>");
    puts("");
    puts("Note: You may only use absolute paths");
    puts("");
    puts("Examples:");
    puts("  mount /dev/sda1 /mnt ext4");
    puts("  mount /dev/nvme0n1p1 /mnt fat32");
    puts("");
    puts("Supported options:");
    puts("  -h, --help    Show this help");
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

    if (argc < 4) {
        printf("mount: missing arguments\n\n");
        usage();
        return 1;
    }

    const char* source = argv[1];
    const char* target = argv[2];
    const char* fstype = argv[3];

    int64_t ret = sys_mount(
        (uint64_t)source,
        (uint64_t)target,
        (uint64_t)fstype,
        0, 0, 0
    );


    if (ret < 0) {
        if (ret == -EBUSY) {
            printf("Mount denied: device already mounted\n");
        } else if (ret == -EINVAL) {
            printf("Mount failed: invalid filesystem\n");
        } else if (ret == -ENODEV) {
            printf("Mount failed: no such blockdev\n");
        } else {
            printf("Mount failed (error=%ld)\n", ret);
        }
        return 1;
    }

    printf("mounted %s -> %s (%s)\n", source, target, fstype);
    return 0;
}