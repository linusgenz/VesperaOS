// alltypes.h
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
#ifndef VESPLIB_ALLTYPES_H
#define VESPLIB_ALLTYPES_H

#include <vespera/time.h>

#define _Addr long
#define _Int64 long
#define _Reg long
#define __BYTE_ORDER 1234
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#define __LONG_MAX 0x7fffffffffffffffL

typedef unsigned long size_t;
typedef unsigned long uintptr_t;
typedef long ptrdiff_t;
typedef long intptr_t;
typedef long regoff_t;
typedef long register_t;
typedef long suseconds_t;

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef int64_t ssize_t;
#endif

typedef unsigned mode_t;
typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

typedef __WINT_TYPE__ wint_t;
typedef unsigned long wctype_t;

#ifndef __cplusplus
typedef int wchar_t;
#endif

#if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 2
typedef long double float_t;
typedef long double double_t;
#else
typedef float float_t;
typedef double double_t;
#endif

typedef void *timer_t;
typedef int clockid_t;
typedef long clock_t;

typedef int pid_t;
typedef unsigned id_t;
typedef unsigned uid_t;
typedef unsigned gid_t;
typedef int key_t;
typedef unsigned useconds_t;

typedef __builtin_va_list va_list;
typedef __builtin_va_list __isoc_va_list;

typedef struct {
    unsigned __opaque1, __opaque2;
} mbstate_t;
typedef struct __locale_struct *locale_t;

struct iovec {
    void *iov_base;
    size_t iov_len;
};
struct winsize {
    unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel;
};

typedef unsigned socklen_t;
typedef unsigned short sa_family_t;

#undef _Addr
#undef _Int64
#undef _Reg

#endif  // VESPLIB_ALLTYPES_H
