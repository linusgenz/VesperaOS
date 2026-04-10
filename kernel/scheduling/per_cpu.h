// per_cpu.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 10.04.26.
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
#ifndef VESPERAOS_PER_CPU_H
#define VESPERAOS_PER_CPU_H

#include "../kernel/acpi/madt.h"
#include <../kernel/units/unit.h>
#include <vespera/types.h>
#include <arch/x86_64/cpu/msr.h>

struct GsData {
    execution_context_t* current_ctx;
    u64 cpu_id;
};

extern GsData g_per_cpu[MAX_CPU_CORES];

inline void per_cpu_init(u8 cpu_id) {
    g_per_cpu[cpu_id].cpu_id = cpu_id;
    g_per_cpu[cpu_id].current_ctx = nullptr;
    wrmsr(MSR_KERNEL_GS_BASE, reinterpret_cast<u64>(&g_per_cpu[cpu_id]));
}

#endif  // VESPERAOS_PER_CPU_H
