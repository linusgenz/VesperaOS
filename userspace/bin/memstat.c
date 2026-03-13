// memstat.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 17.11.25.
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

#include <fflags.h>
#include <realm.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sysstd.h>
#include <vespera/dev/meminfo.h>

#include "stddef.h"
#include "stdint.h"

int main(int argc, char **argv) {
    FILE_HANDLE hdl = open("/dev/meminfo", O_RDONLY);
    if (hdl < 0) {
        printf("memstat: cannot open /dev/meminfo (hdl=%lld)\n", (long long) hdl);
        return -1;
    }

    meminfo_t info;
    ssize_t r = read(hdl, &info, sizeof(info));

    close(hdl);

    if (r != sizeof(info)) {
        printf("memstat: failed to read meminfo (bytes=%lld)\n", (long long) r);
        return -1;
    }

    printf("Memory Statistics:\n");
    printf("  Total: %llu MB\n", info.total_ram / 1024 / 1024);
    printf("  Used : %llu MB\n", info.used_ram  / 1024 / 1024);
    printf("  Free : %llu MB\n", info.free_ram  / 1024 / 1024);

    return 0;
}