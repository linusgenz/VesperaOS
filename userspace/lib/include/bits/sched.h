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
#ifndef _BITS_SCHED_H
#define _BITS_SCHED_H

#include <vespera/sched.h>

#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

#define CPU_ZERO(set) CPU_ZERO_S(sizeof(cpu_set_t), (set))

#define CPU_SET(cpu, set) CPU_SET_S((cpu), sizeof(cpu_set_t), (set))

#define CPU_CLR(cpu, set) CPU_CLR_S((cpu), sizeof(cpu_set_t), (set))

#define CPU_ISSET(cpu, set) CPU_ISSET_S((cpu), sizeof(cpu_set_t), (set))

#define CPU_COUNT(set) CPU_COUNT_S(sizeof(cpu_set_t), (set))

#define CPU_ZERO_S(setsize, set)                                     \
    do {                                                             \
        size_t __i;                                                  \
        cpu_set_t* __s = (set);                                      \
        for (__i = 0; __i < __CPU_WORDS((setsize) * 8); __i++)       \
            __s->__bits[__i] = 0UL;                                  \
    } while (0)

#define CPU_SET_S(cpu, setsize, set)                                          \
    do {                                                                      \
        size_t __c = (size_t)(cpu);                                           \
        if (__c < (setsize) * 8) (set)->__bits[__CPU_WORD(__c)] |= __CPU_MASK(__c); \
    } while (0)

#define CPU_CLR_S(cpu, setsize, set)                                            \
    do {                                                                        \
        size_t __c = (size_t)(cpu);                                             \
        if (__c < (setsize) * 8) (set)->__bits[__CPU_WORD(__c)] &= ~__CPU_MASK(__c); \
    } while (0)

#define CPU_ISSET_S(cpu, setsize, set) \
    (((size_t)(cpu) < (setsize) * 8) ? ((set)->__bits[__CPU_WORD(cpu)] & __CPU_MASK(cpu)) != 0 : 0)

#define CPU_COUNT_S(setsize, set) ({ \
size_t __i, __count = 0; \
const cpu_set_t* __s = (set); \
for (__i = 0; __i < __CPU_WORDS((setsize) * 8); __i++) \
__count += (size_t)__builtin_popcountl(__s->__bits[__i]); \
__count; \
})

#endif //_BITS_SCHED_H
