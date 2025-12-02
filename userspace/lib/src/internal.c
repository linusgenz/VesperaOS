// internal.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 22.09.25.
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
#include <stdbool.h>

size_t uint_to_str(uint64_t value, char *buffer, uint8_t base, bool prefix) {
    const char *digits = "0123456789ABCDEF";
    char temp[32];
    int i = 0;

    if (value == 0) {
        temp[i++] = '0';
    } else {
        while (value > 0) {
            temp[i++] = digits[value % base];
            value /= base;
        }
    }

    int j = 0;
    if (prefix && base == 16) {
        buffer[j++] = '0';
        buffer[j++] = 'x';
    }

    while (i--) {
        buffer[j++] = temp[i];
    }

    buffer[j] = '\0';

    return j;
}

void float_to_str(float val, char *buf, int precision) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    uint32_t int_part = (uint32_t)val;
    float frac_part = val - (float)int_part;

    char int_buf[32];
    uint_to_str(int_part, int_buf, 10, false);
    char *p = int_buf;
    while (*p) {
        *buf++ = *p++;
    }

    *buf++ = '.';

    for (int i = 0; i < precision; i++) {
        frac_part *= 10.0f;
        int digit = (int)frac_part;
        *buf++ = '0' + digit;
        frac_part -= digit;
    }

    *buf = '\0';
}