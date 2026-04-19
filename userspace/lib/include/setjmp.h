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

    /* jmp_buf structure for x86-64
     * Stores all callee-saved registers and stack pointer
     */
    typedef struct {
        unsigned long rbx;      /* Callee-saved register */
        unsigned long rbp;      /* Frame pointer */
        unsigned long r12;      /* Callee-saved register */
        unsigned long r13;      /* Callee-saved register */
        unsigned long r14;      /* Callee-saved register */
        unsigned long r15;      /* Callee-saved register */
        unsigned long rsp;      /* Stack pointer */
        unsigned long rip;      /* Return address */
    } jmp_buf[1];

    /* Save the current execution context
     * Returns 0 when first called
     * Returns non-zero value when returning via longjmp
     */
    int setjmp(jmp_buf env);

    /* Restore execution context
     * Does not return normally - jumps back to setjmp location
     * val: value to be returned by setjmp (must be non-zero)
     */
    _Noreturn void longjmp(jmp_buf env, int val);


#ifdef __cplusplus
}
#endif

#endif //VESPERAOS_SETJMP_H
