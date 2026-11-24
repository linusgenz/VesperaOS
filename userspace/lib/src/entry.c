// entry.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.06.26.
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

extern int main(int argc, char **argv, char **envp);

typedef void (*init_fn_t)(void);

extern init_fn_t __preinit_array_start[] __attribute__((weak));
extern init_fn_t __preinit_array_end[]   __attribute__((weak));
extern init_fn_t __init_array_start[]    __attribute__((weak));
extern init_fn_t __init_array_end[]      __attribute__((weak));
extern init_fn_t __fini_array_start[]    __attribute__((weak));
extern init_fn_t __fini_array_end[]      __attribute__((weak));

static void run_array(init_fn_t *start, init_fn_t *end) {
    if (!start || !end) return;
    uintptr_t s = (uintptr_t)start;
    uintptr_t e = (uintptr_t)end;
    if (e <= s) return;

    size_t count = (e - s) / sizeof(init_fn_t);
    for (size_t i = 0; i < count; i++) {
        if (start[i]) start[i]();
    }
}

static void run_array_reverse(init_fn_t *start, init_fn_t *end) {
    if (!start || !end) return;
    uintptr_t s = (uintptr_t)start;
    uintptr_t e = (uintptr_t)end;
    if (e <= s) return;

    size_t count = (e - s) / sizeof(init_fn_t);
    for (size_t i = count; i > 0; i--) {
        if (start[i - 1]) start[i - 1]();
    }
}

int start_main(int argc, char **argv, char **envp) {
    run_array(__preinit_array_start, __preinit_array_end);
    run_array(__init_array_start, __init_array_end);

    int exit_code = main(argc, argv, envp);

    run_array_reverse(__fini_array_start, __fini_array_end);
    return exit_code;
}