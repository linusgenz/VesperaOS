// signal.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.04.26.
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
#ifndef VESPLIB_SIGNAL_H
#define VESPLIB_SIGNAL_H

#include <stdint.h>

#include <vespera/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int             sig_atomic_t;
typedef uint64_t        sigset_t;

#define SIGHUP    1   /* Hangup (not implemented, reserved)                */
#define SIGINT    2   /* Interrupt (Ctrl-C)                                */
#define SIGQUIT   3   /* Quit (not implemented, reserved)                  */
#define SIGILL    4   /* Illegal instruction                               */
#define SIGTRAP   5   /* Trace/breakpoint (reserved)                       */
#define SIGABRT   6   /* Abort (reserved)                                  */
#define SIGBUS    7   /* Bus error                                         */
#define SIGFPE    8   /* Floating-point exception                          */
#define SIGKILL   9   /* Kill (cannot be caught)                           */
#define SIGUSR1   10  /* User-defined signal 1                             */
#define SIGSEGV   11  /* Segmentation fault                                */
#define SIGUSR2   12  /* User-defined signal 2                             */
#define SIGPIPE   13  /* Broken pipe                                       */
#define SIGALRM   14  /* Alarm clock                                       */
#define SIGTERM   15  /* Termination signal                                */
#define SIGCHLD   17  /* Child process exited                              */
#define SIGCONT   18  /* Continue stopped process                          */
#define SIGTSTP   20  /* Stop from terminal (Ctrl-Z, can be caught)        */
#define SIGTTIN   21  /* Background read from terminal (reserved)          */
#define SIGTTOU   22  /* Background write to terminal (reserved)           */

#define SIGSYS   31  /* Signal System Call                                */

#define NSIG      32  /* Number of supported signals                       */

#define SIG_ERR  ((void (*)(int))-1)  /* Error return from signal()        */

/* ── SA_* flags (sa_flags in struct sigaction) ─────────────────────────── */

#define SA_RESTART   0x01   /* Restart syscalls after signal handling       */
#define SA_NODEFER   0x02   /* Do not block signal during handler execution */
#define SA_RESETHAND 0x04   /* Reset handler to SIG_DFL after invocation    */

/*
 * Layout must match uapi/vespera/signal.h (sigaction_t):
 *   handler @ offset 0
 *   mask    @ offset 8
 */
struct sigaction {
    void     (*sa_handler)(int);  /* Handler function, or SIG_DFL/SIG_IGN   */
    sigset_t   sa_mask;           /* Additional signals to block in handler */
    int        sa_flags;          /* SA_* flags                             */
};

#define sigemptyset(s)       (*(s) = 0,                      0)
#define sigfillset(s)        (*(s) = ~(sigset_t)0,           0)
#define sigaddset(s, sig)    (*(s) |=  (sigset_t)1 << (sig), 0)
#define sigdelset(s, sig)    (*(s) &= ~((sigset_t)1 << (sig)), 0)
#define sigismember(s, sig)  (!!(*(s) & ((sigset_t)1 << (sig))))

/**
 * @brief Register a signal handler (simplified POSIX-style API).
 *
 * @param signum  Signal number (e.g., SIGINT, SIGTERM, …)
 * @param handler New handler, or SIG_DFL / SIG_IGN.
 * @return Previous handler, or SIG_ERR on error.
 */
void (*signal(int signum, void (*handler)(int)))(int);

/**
 * @brief Register a signal handler with extended control.
 *
 * @param signum Signal number.
 * @param act    New action (may be NULL).
 * @param oldact Previous action (may be NULL).
 * @return 0 on success, -1 on error (errno is set).
 */
int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact);

/**
 * @brief Send a signal to the current process.
 *
 * @param signum Signal number.
 * @return 0 on success, -1 on error.
 */
int raise(int signum);

/**
 * @brief Send a signal to another process.
 *
 * @param pid    Target process ID.
 * @param signum Signal number.
 * @return 0 on success, -1 on error (errno is set).
 */
int kill(int pid, int signum);

int sigprocmask(int how, const sigset_t *__restrict__ set, sigset_t *__restrict__ oset);

#ifdef __cplusplus
}
#endif


#endif  // VESPLIB_SIGNAL_H
