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

struct sigaction_t {
     void (*handler)(int);
     u64 sa_mask;
     int sa_flags;
};

// How arguments for sigprocmask
#define SIG_BLOCK   0   /* Union: mask = mask | set */
#define SIG_UNBLOCK 1   /* Remove intersection: mask = mask & ~set */
#define SIG_SETMASK 2   /* Overwrite: mask = set */

#endif  // VESPERAOS_SIGNAL_H
