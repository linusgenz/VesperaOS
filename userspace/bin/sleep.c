// sleep.c
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void usage(void) {
    puts("Usage: sleep <seconds>");
    puts("       sleep --help");
    puts("");
    puts("Pause for SECONDS seconds. Supports decimal values like 0.5 or 1.5.");
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int parse_duration(const char* str, uint64_t* out_ms) {
    if (!str || !*str) return -1;

    uint64_t whole_seconds = 0;
    uint64_t fractional_ms = 0;
    int has_dot = 0;
    int fractional_digits = 0;

    // Parse whole seconds
    while (*str && *str != '.') {
        if (!is_digit(*str)) return -1;
        whole_seconds = whole_seconds * 10 + (*str - '0');
        str++;
    }

    // Parse fractional part if present
    if (*str == '.') {
        has_dot = 1;
        str++;
        uint64_t place = 100;  // 0.1s = 100ms

        while (*str && fractional_digits < 3) {
            if (!is_digit(*str)) return -1;
            fractional_ms += (*str - '0') * place;
            place /= 10;
            str++;
            fractional_digits++;
        }

        // Skip remaining digits
        while (*str) {
            if (!is_digit(*str)) return -1;
            str++;
        }
    }

    // Check for trailing garbage
    if (*str) return -1;

    // Must have at least one digit
    if (whole_seconds == 0 && fractional_ms == 0 && !has_dot) {
        // Could be "0" which is valid
        const char* p = str - (has_dot ? 2 : 1);
        while (*p) p++;
        // Already validated above
    }

    *out_ms = whole_seconds * 1000 + fractional_ms;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage();
        return 0;
    }

    uint64_t ms;
    if (parse_duration(argv[1], &ms) < 0) {
        printf("sleep: invalid time interval '%s'\n", argv[1]);
        return 1;
    }

    timespec_t ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    const int64_t result = nanosleep(&ts, NULL);

    if (result < 0) {
        printf("sleep: error: %s\n", strerror((int)result));
        return 1;
    }

    return 0;
}