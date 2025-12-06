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
    return (static_cast<uint64_t>(high) << 32) | low;
}

void syscall_init() {
    constexpr uint64_t user_cs = 0x23;
    constexpr uint64_t kernel_cs = 0x08;
    constexpr uint64_t star = ((user_cs - 0x10) << 48) | (kernel_cs << 32);
    write_msr(MSR_STAR, star);

    write_msr(MSR_LSTAR, reinterpret_cast<uint64_t>(&syscall_entry));

    write_msr(MSR_FMASK, 0x0000000000000000); // TEMP not secure. mask everything later TODO

    uint64_t efer = read_msr(MSR_EFER);
    efer |= EFER_SCE;
    write_msr(MSR_EFER, efer); // enable syscalls
}

#define MSR_GS_BASE 0xC0000101
