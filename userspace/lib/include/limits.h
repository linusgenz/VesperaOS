/**
 * @file limits.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 02.01.26.
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
#ifndef _LIMITS_H
#define _LIMITS_H

#define PATH_MAX 4096

#define RE_DUP_MAX 255

#define CHARCLASS_NAME_MAX 14

#define CHAR_BIT 8

/* signed char */
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127

/* unsigned char */
#define UCHAR_MAX 255

#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX

/* signed short */
#define SHRT_MIN (-32768)
#define SHRT_MAX 32767

/* unsigned short */
#define USHRT_MAX 65535

/* signed int */
#define INT_MIN (-2147483647-1)
#define INT_MAX 2147483647

/* unsigned int */
#define UINT_MAX 4294967295U

/* signed long */
#if defined(__x86_64__) || defined(__aarch64__) || defined(__LP64__)
#define LONG_MIN (-9223372036854775807L-1L)
#define LONG_MAX 9223372036854775807L
#define ULONG_MAX 18446744073709551615UL
#else
#define LONG_MIN (-2147483647L-1L)
#define LONG_MAX 2147483647L
#define ULONG_MAX 4294967295UL
#endif

/* signed long long (C99) */
#define LLONG_MIN (-9223372036854775807LL-1LL)
#define LLONG_MAX 9223372036854775807LL
#define ULLONG_MAX 18446744073709551615ULL

#if defined(__x86_64__) || defined(__aarch64__) || defined(__LP64__)
#define UINTPTR_MAX ULONG_MAX
#else
#define UINTPTR_MAX UINT_MAX
#endif

#define MB_LEN_MAX 16

#endif //VESPERAOS_LIMITS_H