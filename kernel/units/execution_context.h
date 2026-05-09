// execution_context.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.05.26.
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

#ifndef VESPERAOS_KERNEL_UNITS_EXECUTION_CONTEXT_H
#define VESPERAOS_KERNEL_UNITS_EXECUTION_CONTEXT_H

#include <vespera/mm/addr.h>
#include <vespera/types.h>

#include "../scheduling/unit_context.h"

// The six System V AMD64 integer argument registers.
// Used to pass initial arguments to a unit's entry point.
struct ArgRegisters {
    u64 rdi, rsi, rdx, rcx, r8, r9;
};

/**
 ExecutionContext — all runtime state needed to schedule and resume
 a unit.  Lives inside struct Unit.

 Layout is split into four logical sections:

   Kernel stack       — the kernel-side stack used during syscalls/interrupts.
   User stack         — the userspace stack and its physical backing.
   Entry point & args — entry point of kernel units and initial args
   CPU state          — register context, FPU state, trap frame.

 @warning  Syscall assembly stubs reference fields in this struct by
           hard-coded byte offsets.  Do NOT reorder or insert fields
           without updating those stubs.  New fields must be appended
           at the END of the relevant section, or at the very end of
           the struct.

 @warning  Field ORDER IS FIXED — assembly syscall stubs use hard-coded
           byte offsets into this struct. Never reorder or insert fields.
           New fields must be appended at the very end of the struct.
           Section labels below are documentation only.
*/
struct ExecutionContext {
    // Kernel stack
    u64 stack_size;
    virt_addr_t stack;
    virt_addr_t stack_top;
    virt_addr_t stack_pointer;

    // User stack (VA slot)
    u64 user_stack_size;
    virt_addr_t user_stack;
    virt_addr_t user_stack_top;
    virt_addr_t user_stack_pointer;

    // Entry point & initial arguments
    void (*entry)(void*);
    ArgRegisters regs;
    void* arg;

    // User stack physical backing
    // Needed for teardown (unmap + free_pages_phys)
    phys_addr_t user_stack_phys;
    virt_addr_t user_stack_virt_base;

    // CPU state (register context, FPU, trap frame)
    TrapFrame current_trap_frame;
    UnitCpuContext cpu_ctx;
    UnitFpuState fpu_ctx;
    u64 fs_base;
};

#endif  // VESPERAOS_KERNEL_UNITS_EXECUTION_CONTEXT_H
