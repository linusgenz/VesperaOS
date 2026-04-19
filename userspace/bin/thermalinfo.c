// thermalinfo.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.04.26.
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

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sysstd.h>
#include <vespera/dev/thermal.h>

#include "../../sysroot/usr/include/vespera/fflags.h"

/// Writes an unsigned decimal integer into @p buf (null-terminated).
/// @p buf must be at least 12 bytes.
void uint32_t_to_str(uint32_t val, char* buf, size_t buf_len) {
    if (buf_len == 0) return;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[12];
    size_t i = 0;
    while (val > 0 && i < sizeof(tmp) - 1) {
        tmp[i++] = (char)('0' + (val % 10));
        val /= 10;
    }
    size_t out = 0;
    while (i > 0 && out < buf_len - 1) {
        buf[out++] = tmp[--i];
    }
    buf[out] = '\0';
}

/// printfs a temperature given in milli-Celsius as "XX.X °C".
void printf_temp(uint32_t mc) {
    uint32_t whole = mc / 1000u;
    uint32_t decimal = (mc % 1000u) / 100u;  // one decimal place
    char buf[16];
    uint32_t_to_str(whole, buf, sizeof(buf));
    printf(buf);
    printf(".");
    uint32_t_to_str(decimal, buf, sizeof(buf));
    printf(buf);
    printf(" °C");
}

/// printfs a fixed-width string padded with spaces to @p width.
void printf_padded(const char* s, size_t width) {
    size_t len = 0;
    while (s[len]) len++;
    printf(s);
    for (size_t i = len; i < width; i++) printf(" ");
}

const char* source_str(thermal_source_t src) {
    switch (src) {
        case THERMAL_SOURCE_MSR:
            return "MSR";
        case THERMAL_SOURCE_ACPI:
            return "ACPI";
        default:
            return "?";
    }
}

int main() {
    HANDLE hdl = open("/dev/thermal", O_RDONLY);
    if (hdl < 0) {
        puts("thermalinfo: cannot open /dev/thermal");
        return 1;
    }

    thermal_info_t info ;
    ssize_t n = read(hdl, &info, sizeof(info));
    close(hdl);

    if (n < (ssize_t)(sizeof(info))) {
        puts("thermalinfo: short read from /dev/thermal");
        return 1;
    }

    if (info.zone_count == 0) {
        puts("thermalinfo: no thermal zones reported");
        return 0;
    }

    puts("Thermal zones:");
    puts("  Zone            Temp       Critical   Source");
    puts("  --------------- ---------- ---------- ------");

    for (uint32_t i = 0; i < info.zone_count; i++) {
        const thermal_zone_t z = info.zones[i];
        printf("  ");
        printf_padded(z.name, 15);
        printf("  ");

        // Temperature
        {
            char tmp[24];
            // Build the temp string into a local buffer by redirecting
            // printf_temp logic inline so we can pad it.
            uint32_t whole = z.temp_mc / 1000u;
            uint32_t decimal = (z.temp_mc % 1000u) / 100u;
            uint32_t_to_str(whole, tmp, sizeof(tmp));
            size_t tlen = 0;
            while (tmp[tlen]) tlen++;
            // append ".X °C"
            tmp[tlen++] = '.';
            tmp[tlen++] = (char)('0' + decimal);
            tmp[tlen++] = ' ';
            tmp[tlen++] = '\xc2';
            tmp[tlen++] = '\xb0';  // UTF-8 degree sign
            tmp[tlen++] = 'C';
            tmp[tlen] = '\0';
            printf_padded(tmp, 10);
        }
        printf("  ");

        // Critical
        if (z.crit_mc > 0) {
            char tmp[24];
            uint32_t whole = z.crit_mc / 1000u;
            uint32_t decimal = (z.crit_mc % 1000u) / 100u;
            uint32_t_to_str(whole, tmp, sizeof(tmp));
            size_t tlen = 0;
            while (tmp[tlen]) tlen++;
            tmp[tlen++] = '.';
            tmp[tlen++] = (char)('0' + decimal);
            tmp[tlen++] = ' ';
            tmp[tlen++] = '\xc2';
            tmp[tlen++] = '\xb0';
            tmp[tlen++] = 'C';
            tmp[tlen] = '\0';
            printf_padded(tmp, 10);
        } else {
            printf_padded("n/a", 10);
        }
        printf("  ");
        puts(source_str(z.source));
    }

    return 0;
}