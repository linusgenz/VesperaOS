// signal.h
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
#ifndef VESPERAOS_SIGNAL_H
#define VESPERAOS_SIGNAL_H

#include <vespera/types.h>

#define SIG_DFL ((void(*)(int))0) /* Default signal handling           */
#define SIG_IGN ((void(*)(int))1) /* Ignore signal                     */

typedef struct {
     int si_signo;
     int si_code;
     int si_errno;
     union {
          void *si_addr;
          int   si_status;
          int   si_pid;
     };
} siginfo_t;

struct sigaction {
     union {
          void (*sa_handler)(int);
          void (*sa_sigaction)(int, siginfo_t *, void *);
     };
     sigset_t sa_mask;
     int      sa_flags;
};


// How arguments for sigprocmask
#define SIG_BLOCK   0   /* Union: mask = mask | set */
#define SIG_UNBLOCK 1   /* Remove intersection: mask = mask & ~set */
#define SIG_SETMASK 2   /* Overwrite: mask = set */

/* ── SA_* flags (sa_flags in struct sigaction) ─────────────────────────── */

//#define SA_RESTART   0x10000000   /* Restart syscalls after signal handling       */ // NOOP
#define SA_NODEFER   0x40000000   /* Do not block signal during handler execution */
//#define SA_RESETHAND 0x80000000 /* Reset to SIG_DFL on entry to handler.  */ // NOOP

//#define SA_NOCLDSTOP  1           /* Don't send SIGCHLD when children stop.  */ // NOOP
//#define SA_NOCLDWAIT  2        /* Don't create zombie on child death.  */ // NOOP
#define SA_SIGINFO    4        /* Invoke signal-catching function with
three arguments instead of one.  */

#endif  // VESPERAOS_SIGNAL_H
