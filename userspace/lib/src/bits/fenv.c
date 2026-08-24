// fenv.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 06.06.26.
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

#include <stdint.h>
#include <bits/fenv.h>

#define FE_TONEAREST  0x0000
#define FE_DOWNWARD   0x2000
#define FE_UPWARD     0x4000
#define FE_TOWARDZERO 0x6000
#define ROUND_MASK    0x6000

int fegetround(void) {
    uint32_t mxcsr;
    __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
    return mxcsr & ROUND_MASK;
}

int fesetround(int mode) {
    if (mode & ~ROUND_MASK) return -1;
    uint32_t mxcsr;
    __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
    mxcsr = (mxcsr & ~ROUND_MASK) | mode;
    __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
    return 0;
}

#define FE_INVALID    0x01
#define FE_DENORMAL   0x02   /* nicht Teil des Standards, aber MXCSR-Bit vorhanden */
#define FE_DIVBYZERO  0x04
#define FE_OVERFLOW   0x08
#define FE_UNDERFLOW  0x10
#define FE_INEXACT    0x20

#define FE_ALL_EXCEPT (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT)

static inline uint32_t get_mxcsr(void) {
    uint32_t mxcsr;
    __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
    return mxcsr;
}

static inline void set_mxcsr(uint32_t mxcsr) {
    __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
}

int feraiseexcept(int excepts) {
    excepts &= FE_ALL_EXCEPT;
    if (excepts == 0)
        return 0;

    uint32_t mxcsr = get_mxcsr();

    mxcsr |= excepts;
    set_mxcsr(mxcsr);

    return 0;
}

int feclearexcept(int excepts) {
    excepts &= FE_ALL_EXCEPT;
    uint32_t mxcsr = get_mxcsr();
    mxcsr &= ~excepts;
    set_mxcsr(mxcsr);
    return 0;
}

int fetestexcept(int excepts) {
    excepts &= FE_ALL_EXCEPT;
    return get_mxcsr() & excepts;
}