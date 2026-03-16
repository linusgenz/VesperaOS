// math.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.03.26.
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
#ifndef VESPERAOS_MATH_H
#define VESPERAOS_MATH_H

extern "C" {

#if 100 * __GNUC__ + __GNUC_MINOR__ >= 303
#define NAN __builtin_nanf("")
#define INFINITY __builtin_inff()
#else
#define NAN (0.0f / 0.0f)
#define INFINITY 1e5000f
#endif

#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4

static __inline unsigned __FLOAT_BITS(float __f) {
    union {
        float __f;
        unsigned __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}
static __inline unsigned long long __DOUBLE_BITS(double __f) {
    union {
        double __f;
        unsigned long long __i;
    } __u;
    __u.__f = __f;
    return __u.__i;
}

int __fpclassify(double);
int __fpclassifyf(float);
int __fpclassifyl(long double);

#define fpclassify(x) \
    (sizeof(x) == sizeof(float) ? __fpclassifyf(x) : sizeof(x) == sizeof(double) ? __fpclassify(x) : __fpclassifyl(x))

#define isinf(x)                                                                       \
    (sizeof(x) == sizeof(float)    ? (__FLOAT_BITS(x) & 0x7fffffff) == 0x7f800000      \
     : sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL >> 1) == 0x7ffULL << 52 \
                                   : __fpclassifyl(x) == FP_INFINITE)

#define isnan(x)                                                                      \
    (sizeof(x) == sizeof(float)    ? (__FLOAT_BITS(x) & 0x7fffffff) > 0x7f800000      \
     : sizeof(x) == sizeof(double) ? (__DOUBLE_BITS(x) & -1ULL >> 1) > 0x7ffULL << 52 \
                                   : __fpclassifyl(x) == FP_NAN)

double floor(double x);

double sqrtf(double x);

double sqrt(double x);

double pow(double x, double y);

double ceil(double x);

double fabs(double x);

double fmod(double x, double y);

double acos(double x);

double cos(double x);

double scalbn(double x, int n);

}

#endif  // VESPERAOS_MATH_H
