// shell.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
//
// This file is part of LuminOS.
// 
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#define STDIN_FD 0
#define STDOUT_FD 1
#define MAX_INPUT 128

#include "stdint.h"

int64_t syscall(
    uint64_t num,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    int64_t ret = -1;

    register uint64_t r10_ asm("r10") = arg3;
    register uint64_t r8_  asm("r8")  = arg4;
    register uint64_t r9_  asm("r9")  = arg5;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2),
          "r"(r10_), "r"(r8_), "r"(r9_)
        : "rcx", "r11", "memory"
    );

    return ret;
}


static int64_t sys_read(int32_t fd, void* buf, uint64_t count) {
    return syscall(0, fd, (long)buf, count, 0, 0, 0);
}

static int64_t sys_write(int32_t fd, const void* buf, uint64_t count) {
    return syscall(1, 1, (long)buf, count, 0, 0, 0);
}

void print(const char* msg) {
    while (*msg) {
        sys_write(STDOUT_FD, msg, 1);
        msg++;
    }
}

int strncmp(const char* a, const char* b, int max) {
    for (int i = 0; i < max; i++) {
        if (a[i] != b[i]) return a[i] - b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

int strlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

void shell_main() {
    char buf[MAX_INPUT];

    print("Welcome to Minimal Shell!\n");

    while (1) {
        print("> ");

        int n = sys_read(STDIN_FD, buf, MAX_INPUT - 1);
        if (n <= 0) continue;
        buf[n] = '\0';

        // Strip newline
        if (buf[n - 1] == '\n') buf[n - 1] = '\0';

        if (strncmp(buf, "hello", 5) == 0) {
            print("Hello, world!\n");
        } else if (strncmp(buf, "exit", 4) == 0) {
            print("Bye!\n");
            break;
        } else {
            print("Unknown command: ");
            print(buf);
            print("\n");
        }
    }
}

void _start() {
    shell_main();
    syscall(60, 0, 0, 0, 0, 0, 0);
}



