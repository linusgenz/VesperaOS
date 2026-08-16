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

struct sigaction_t;
class Unit;
enum class Signal : u32 {
    SIGINT = 2,
    SIGILL = 4,
    SIGBUS = 7,
    SIGFPE = 8,
    SIGKILL = 9,
    SIGSEGV = 11,
    SIGPIPE = 13,
    SIGALRM = 14,
    SIGTERM = 15,
    SIGCHLD = 17,
    SIGTSTP = 20,
    SIGUSR1 = 10,
    SIGUSR2 = 12,
    SIGCONT = 18,
    SIGTTOU = 22,
    SIGTTIN = 21,
    SIGSYS  = 31,
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

i64 signal_update_mask(Unit* u, int how, const u64* new_set, u64* old_set);

/**
 * @brief Installs a signal action for the given unit.
 *
 * Validates @p signum, rejects SIGKILL, and updates the unit's
 * signal_actions table and signal mask from @p act.
 *
 * @return 0 on success, negative errno on failure.
 */
[[nodiscard]] i64 signal_set_action(Unit* u, i32 signum, const sigaction_t* act);

/**
 * @brief Restores the pre-signal register state after a handler returns.
 *
 * Reads the @ref SignalFrame pushed by @ref signal_setup_userframe from
 * the unit's user stack and overwrites the current trap frame with it.
 *
 * @return -EINTR always — the restored context resumes via iretq, the
 *         syscall return value is discarded.
 * @return -EINVAL if the unit has no current trap frame.
 * @return -EFAULT if the frame address is outside the unit's stack.
 */
[[nodiscard]] i64 signal_restore_frame(Unit* u);

#endif  // VESPERAOS_SIGNALS_H
