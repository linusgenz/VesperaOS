// unit_context.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.04.26.
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
#ifndef VESPERAOS_UNIT_CONTEXT_H
#define VESPERAOS_UNIT_CONTEXT_H

#include <vespera/types.h>

struct UnitCpuContext {
    u64 r15, r14, r13, r12;
    u64 r11, r10, r9,  r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;

    // iret frame
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};


struct alignas(16) UnitFpuState {
    u8 fxsave_area[512];
};

inline void fpu_save(UnitFpuState* s) {
    asm volatile("fxsaveq %0" : "=m"(s->fxsave_area) :: "memory");
}

inline void fpu_restore(const UnitFpuState* s) {
    asm volatile("fxrstorq %0" :: "m"(s->fxsave_area) : "memory");
}

inline void fpu_init_state(UnitFpuState* s) {
    for (usize i = 0; i < 512; ++i) s->fxsave_area[i] = 0;
    // FCW @ offset 0: 0x037F — mask all x87 exceptions, round-to-nearest, 64-bit prec
    s->fxsave_area[0] = 0x7F;
    s->fxsave_area[1] = 0x03;
    // MXCSR @ offset 24: 0x1F80 — mask all SSE exceptions, round-to-nearest
    s->fxsave_area[24] = 0x80;
    s->fxsave_area[25] = 0x1F;
}

struct TrapFrame;

inline void cpu_context_save(const TrapFrame* tf, UnitCpuContext* ctx) {
    ctx->r15    = tf->r15;    ctx->r14 = tf->r14; ctx->r13 = tf->r13; ctx->r12 = tf->r12;
    ctx->r11    = tf->r11;    ctx->r10 = tf->r10; ctx->r9  = tf->r9;  ctx->r8  = tf->r8;
    ctx->rbp    = tf->rbp;    ctx->rdi = tf->rdi; ctx->rsi = tf->rsi; ctx->rdx = tf->rdx;
    ctx->rcx    = tf->rcx;    ctx->rbx = tf->rbx; ctx->rax = tf->rax;
    ctx->rip    = tf->rip;    ctx->cs  = tf->cs;  ctx->rflags = tf->rflags;
    ctx->rsp    = tf->rsp;    ctx->ss  = tf->ss;
}

inline void cpu_context_load(const UnitCpuContext* ctx, TrapFrame* tf) {
    tf->r15    = ctx->r15;    tf->r14 = ctx->r14; tf->r13 = ctx->r13; tf->r12 = ctx->r12;
    tf->r11    = ctx->r11;    tf->r10 = ctx->r10; tf->r9  = ctx->r9;  tf->r8  = ctx->r8;
    tf->rbp    = ctx->rbp;    tf->rdi = ctx->rdi; tf->rsi = ctx->rsi; tf->rdx = ctx->rdx;
    tf->rcx    = ctx->rcx;    tf->rbx = ctx->rbx; tf->rax = ctx->rax;
    tf->rip    = ctx->rip;    tf->cs  = ctx->cs;  tf->rflags = ctx->rflags;
    tf->rsp    = ctx->rsp;    tf->ss  = ctx->ss;
}

#endif  // VESPERAOS_UNIT_CONTEXT_H
