// gpuinfo.c
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
// You should have have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fflags.h>
#include <sysstd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <vespera/dev/ioctl_devinfo.h>

static void print_field(const char* label, const char* value) {
    if (value && value[0]) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-12s%s", label, value);
        puts(buf);
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    HANDLE hdl = open("/dev/gpu", O_RDONLY);
    if (hdl < 0) {
        puts("Failed to open /dev/gpu");
        return 1;
    }

    devinfo_t info;
    long n = ioctl(hdl, IOCTL_DEVINFO_GET_ALL, &info);
    if (n < 0) {
        printf("Failed to get GPU info: %s", strerror(n));
        close(hdl);
        return 1;
    }

    close(hdl);

    puts("GPU Information");
    puts("─────────────────────────");
    print_field("Vendor:",   info.vendor);
    print_field("Model:",    info.model);
    print_field("Firmware:", info.firmware);
    print_field("Serial:",   info.serial);

    return 0;
}