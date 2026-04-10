// signals.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.03.26.
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
#ifndef VESPERAOS_SIGNALS_H
#define VESPERAOS_SIGNALS_H

#include <vespera/types.h>

#include "interrupts.h"


class Unit;
enum class Signal : i32 {
    SIGINT  = 2,
    SIGFPE = 8,
    SIGKILL = 9,
    SIGSEGV = 11,
    SIGBUS = 7,
    SIGILL = 4,
    SIGTERM = 15,
    SIGCHLD = 17,
    SIGALRM = 14,
    SIGPIPE = 13,
    SIGUSR1 =  20,
    SIGUSR2 =  21,
};

struct SignalFrame {
    u64 rax, rbx, rcx, rdx;
    u64 rbp, rsi, rdi;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 rip;
    u64 rsp;
    u64 rflags;
    i32 signum;
    u32 _pad;
};

struct SignalAction {
    enum class Disposition : u8 { Default, Ignore, Handler } disposition;
    void (*handler)(int);
};

bool is_valid_signal(i32 signum);
void signal_send(Unit* u, Signal sig);
void signal_dispatch(Unit* u, TrapFrame* trap);
void signal_default(Unit* unit, Signal sig);

#endif  // VESPERAOS_SIGNALS_H
