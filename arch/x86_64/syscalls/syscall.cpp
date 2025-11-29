// syscall.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.08.25.
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

#include "syscall.h"


#include "../../../include/log.h"

extern "C" void syscall_entry();

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084
#define EFER_SCE  1

static void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile ("wrmsr" :: "c"(msr), "a"(low), "d"(high));
}

static uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
    return ((uint64_t) high << 32) | low;
}

void syscall_init() {
    uint64_t star = ((uint64_t) 0x1B << 48) | ((uint64_t) 0x08 << 32);
    write_msr(MSR_STAR, star);

    write_msr(MSR_LSTAR, reinterpret_cast<uint64_t>(&syscall_entry));

    write_msr(MSR_FMASK, 0x0000000000000000); // TEMP not secure. mask everything later TODO

    uint64_t efer = read_msr(MSR_EFER);
    efer |= EFER_SCE;
    write_msr(MSR_EFER, efer); // enable syscalls
}

#define MSR_GS_BASE 0xC0000101


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
    register uint64_t r8_ asm("r8") = arg4;
    register uint64_t r9_ asm("r9") = arg5;

    asm volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg0), "S"(arg1), "d"(arg2),
        "r"(r10_), "r"(r8_), "r"(r9_)
        : "rcx", "r11", "memory"
    );

    return ret;
}