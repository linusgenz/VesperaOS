// syscall_interface.cpp
//
// LuminOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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
#include "syscall_interface.h"
#include "../../include/log.h"

constexpr int MAX_SYSCALLS = 256;
static syscall_fn syscall_table[MAX_SYSCALLS];

void install_syscalls() {
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_table[i] = nullptr;
    }

    syscall_table[SYSCALL_WRITE] = sys_write;
    syscall_table[SYSCALL_EXIT]  = sys_exit;
}

extern "C" void syscall_handler(
    uint64_t num,
    uint64_t arg0,
    uint64_t arg1,
    uint64_t arg2,
    uint64_t arg3,
    uint64_t arg4,
    uint64_t arg5
) {
    uint64_t ret = -1;

    if (num < MAX_SYSCALLS && syscall_table[num]) {
        ret = syscall_table[num](arg0, arg1, arg2, arg3, arg4, arg5);
    } else {
        Log::PrintLn("[SYSCALL] Invalid syscall number: %llu", num);
    }

    asm volatile ("mov %0, %%rax" :: "r"(ret));
}
