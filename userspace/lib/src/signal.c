// signal.c
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

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sysstd.h>

#include <errno.h>

/* sys_sigaction erwartet exakt dieses Layout (uapi/vespera/signal.h):
 *   void (*handler)(int)  @ 0
 *   uint64_t mask         @ 8
 * struct sigaction passt dazu direkt. */

int sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
    (void)oldact;

    if (!act) {
        errno = EINVAL;
        return -1;
    }

    int64_t ret = sys_sigaction((uint64_t)signum, (uint64_t)act, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return 0;
}

void (*signal(int signum, void (*handler)(int)))(int) {
    struct sigaction act = {
        .sa_handler = handler,
        .sa_mask    = 0,
        .sa_flags   = 0,
    };
    if (sigaction(signum, &act, NULL) < 0)
        return SIG_ERR;
    return handler;
}

int raise(int signum) {
    return kill(0, signum);
}

int kill(int pid, int signum) {
    int64_t ret = sys_kill((uint64_t)pid, (uint64_t)signum, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)-ret;
        return -1;
    }
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oset) {
    const int ret = sys_sigprocmask(how, (uint64_t)set, (uint64_t)oset,0,0,0);

    if (ret < 0) {
        errno = -ret;
        return -1;
    }

    return 0;
}