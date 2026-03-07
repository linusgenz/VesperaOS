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

#include <arch/x86_64/cpu/msr.h>

#include <vespera/log.h>

extern "C" void syscall_entry();

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084
#define EFER_SCE 1

void syscall_init() {
    constexpr uint64_t user_cs = 0x23;
    constexpr uint64_t kernel_cs = 0x08;
    constexpr uint64_t star = ((user_cs - 0x10) << 48) | (kernel_cs << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, reinterpret_cast<uint64_t>(&syscall_entry));

    wrmsr(MSR_FMASK, 0x0000000000000000);  // TEMP not secure. mask everything later TODO

    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);  // enable syscalls
}

#define MSR_GS_BASE 0xC0000101
