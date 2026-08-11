// types.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 19.09.25.
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

#ifndef VESPERAOS_TYPES_H
#define VESPERAOS_TYPES_H

#if !defined(__x86_64__)
#error "This kernel targets x86-64 only."
#endif

#if !defined(__cplusplus) || (__cplusplus < 202002L)
#error "This kernel requires C++20 or later."
#endif

typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef float       f32;   // IEEE-754 binary32 (32-bit)
typedef double      f64;   // IEEE-754 binary64 (64-bit)
typedef long double f80;

static_assert(sizeof(f32) == 4, "f32 must be 32-bit");
static_assert(sizeof(f64) == 8, "f64 must be 64-bit");

static_assert(__FLT_RADIX__ == 2, "Binary floating point required");
static_assert(__FLT_MANT_DIG__ == 24, "f32 must be IEEE-754 binary32");
static_assert(__DBL_MANT_DIG__ == 53, "f64 must be IEEE-754 binary64");

typedef __UINTPTR_TYPE__ uptr;
typedef __INTPTR_TYPE__ iptr;

typedef i8 int8_t;
typedef i16 int16_t;
typedef i32 int32_t;
typedef i64 int64_t;

typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;

typedef long int isize;
typedef __SIZE_TYPE__ usize;

typedef isize ssize_t;
typedef usize size_t;
typedef i64     ptrdiff_t;

typedef __attribute__((__aligned__(__BIGGEST_ALIGNMENT__))) struct {
    long long   __ll;
    long double __ld;
} max_align_t;

#if defined(__SIZEOF_INT128__)
typedef __int128 i128;
typedef unsigned __int128 u128;
#endif

#define offsetof(type, member) __builtin_offsetof(type, member)

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

#define SIZE_MAX    U64_MAX
#define PTRDIFF_MIN I64_MIN
#define PTRDIFF_MAX I64_MAX

typedef u64 RealmId;
typedef u64 HandleId;
typedef u64 UnitId;

typedef struct {
    u32 x;
    u32 y;
} point_t;

#endif  // VESPERAOS_TYPES_H
