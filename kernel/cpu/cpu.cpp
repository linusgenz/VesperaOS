// cpu.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 11.03.26.
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

#include "cpu.h"
#include <vespera/mm/memory.h>

bool running_in_qemu = false;

static void cpuid(u32 leaf, u32& eax, u32& ebx,
                  u32& ecx, u32& edx) {
    asm volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(leaf), "c"(0));
}

static bool check_hypervisor_vendor() {
    u32 eax = 0, ebx = 0, ecx = 0, edx = 0;

    cpuid(0x1, eax, ebx, ecx, edx);
    if (!(ecx & (1u << 31)))
        return false;

    cpuid(0x40000000, eax, ebx, ecx, edx);

    char vendor[13] = {};
    memcpy(vendor + 0, &ebx, 4);
    memcpy(vendor + 4, &ecx, 4);
    memcpy(vendor + 8, &edx, 4);

    // QEMU without KVM:  "TCGTCGTCGTCG"
    // QEMU with KVM:   "KVMKVMKVM\0\0\0"
    return (memcmp(vendor, "TCGTCGTCGTCG", 12) == 0 ||
            memcmp(vendor, "KVMKVMKVM",    9) == 0);
}

void detect_qemu() {
    running_in_qemu = check_hypervisor_vendor();
}

bool in_qemu() {
    return running_in_qemu;
}