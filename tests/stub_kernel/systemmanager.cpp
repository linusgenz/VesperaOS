// systemmanager.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 04.03.26.
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

#include "vespera/system/system_manager.h"
#include <csetjmp>
#include <cstdio>
#include <cstdlib>

jmp_buf  g_panic_jmp;
bool     g_panic_armed   = false;
bool     g_panic_fired   = false;
char     g_panic_msg[256] = {};
int32_t  g_panic_code    = 0;

namespace kernel {

    [[noreturn]]
    void SystemManager::system_panic(const char* message, int32_t error_code) {
        if (g_panic_armed) {
            g_panic_fired = true;
            g_panic_code  = error_code;
            if (message)
                snprintf(g_panic_msg, sizeof(g_panic_msg), "%s", message);
            longjmp(g_panic_jmp, 1);
        }
        fprintf(stderr, "UNEXPECTED PANIC: %s (code %d)\n", message ? message : "?", error_code);
        abort();
    }

}