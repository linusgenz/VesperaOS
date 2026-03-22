// sys_sigreturn.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.03.26.
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

#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>
#include <vespera/realm/realm_manager.h>
#include <vespera/scheduling.h>
#include <vespera/signals.h>
#include <vespera_errno.h>

namespace syscalls::internal {
    i64 sys_sigreturn(u64, u64, u64, u64, u64, u64) {
        Unit* u = kernel::scheduling::get_current_unit();
        if (!u) return -EINVAL;

        TrapFrame* trap = &u->context.current_trap_frame;
        const uptr frame_usp = trap->rsp;

        const uptr stack_virt_base = virt_raw(u->context.user_stack_virt_base);
        const uptr stack_hhdm_base = virt_raw(u->context.user_stack);

        if (frame_usp < stack_virt_base ||
            frame_usp + sizeof(SignalFrame) > stack_virt_base + u->context.user_stack_size) {
            return -EFAULT;
        }

        const uptr offset = frame_usp - stack_virt_base;
        const auto* frame = reinterpret_cast<const SignalFrame*>(stack_hhdm_base + offset);

        trap->rax = frame->rax;
        trap->rbx = frame->rbx;
        trap->rcx = frame->rcx;
        trap->rdx = frame->rdx;
        trap->rbp = frame->rbp;
        trap->rsi = frame->rsi;
        trap->rdi = frame->rdi;
        trap->r8 = frame->r8;
        trap->r9 = frame->r9;
        trap->r10 = frame->r10;
        trap->r11 = frame->r11;
        trap->r12 = frame->r12;
        trap->r13 = frame->r13;
        trap->r14 = frame->r14;
        trap->r15 = frame->r15;
        trap->rip = frame->rip;
        trap->rsp = frame->rsp;
        trap->rflags = frame->rflags;

        return 0;
    }
}  // namespace syscalls::internal