// sched.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.08.26.
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

#ifndef _UAPI_VESPERA_SCHED_H
#define _UAPI_VESPERA_SCHED_H

#include <vespera/types.h>

#ifndef CPU_SETSIZE
#define CPU_SETSIZE 1024
#endif

#define __CPU_BITS_PER_WORD (8 * (size_t)sizeof(unsigned long))
#define __CPU_WORDS(setsize) (((setsize) + __CPU_BITS_PER_WORD - 1) / __CPU_BITS_PER_WORD)
#define __CPU_WORD(cpu)      ((size_t)(cpu) / __CPU_BITS_PER_WORD)
#define __CPU_MASK(cpu)      (1UL << ((size_t)(cpu) % __CPU_BITS_PER_WORD))

typedef struct {
    unsigned long __bits[__CPU_WORDS(CPU_SETSIZE)];
} cpu_set_t;

#endif /* _UAPI_VESPERA_SCHED_H */