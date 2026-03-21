// umount.c
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
    puts("  umount <target>");
    puts("");
    puts("Note: You may only use absolute paths");
    puts("");
    puts("Examples:");
    puts("  umount /mnt");
    puts("  umount /mnt/data");
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

    const char* target = argv[1];

    const int64_t ret = umount(target, 0);

    if (ret < 0) {
        if (ret == -EBUSY) {
            printf("Mount failed: device still in use\n");
        } else if (ret == -EINVAL) {
            printf("Mount failed: invalid filesystem\n");
        } else if (ret == -ENODEV) {
            printf("Mount failed: no such blockdev\n");
        } else {
            printf("Mount failed (error=%ld)\n", ret);
        }
        return 1;
    }

    printf("unmounted %s\n", target);
    return 0;
}