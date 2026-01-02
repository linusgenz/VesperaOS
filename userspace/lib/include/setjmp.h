/**
 * @file setjmp.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 01.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_SETJMP_H
#define VESPERAOS_SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        unsigned long rsp;
        unsigned long rbp;
        unsigned long rbx;
        unsigned long r12;
        unsigned long r13;
        unsigned long r14;
        unsigned long r15;
    } jmp_buf[1];

    int setjmp(jmp_buf env);

    void longjmp(jmp_buf env, int val) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif //VESPERAOS_SETJMP_H