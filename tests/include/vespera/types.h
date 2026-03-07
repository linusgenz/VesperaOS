// types.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.03.26.
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
#pragma once
#define VESPERAOS_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char        i8;
typedef signed short       i16;
typedef signed int         i32;
typedef signed long long   i64;

typedef long int isize;
typedef __SIZE_TYPE__ usize;

typedef __UINTPTR_TYPE__ uptr;
typedef __INTPTR_TYPE__ iptr;

#define I8_MIN (-(127) - 1)
#define I8_MAX (127)
#define I16_MIN (-(32767) - 1)
#define I16_MAX (32767)
#define I32_MIN (-(2147483647) - 1)
#define I32_MAX (2147483647)
#define I64_MIN (-(9223372036854775807LL) - 1)
#define I64_MAX (9223372036854775807LL)

#define U8_MAX (255U)
#define U16_MAX (65535U)
#define U32_MAX (4294967295U)
#define U64_MAX (18446744073709551615ULL)


#define IPTR_MIN  I64_MIN
#define IPTR_MAX  I64_MAX
#define UPTR_MAX  U64_MAX