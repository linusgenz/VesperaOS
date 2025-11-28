// stddef.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.11.25.
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

#ifndef VESPERAOS_STDDEF_H
#define VESPERAOS_STDDEF_H

/* Signed type for pointer differences */
typedef long ptrdiff_t;

/* Unsigned type for sizes */
typedef unsigned long size_t;

/* Wide character type */
typedef int wchar_t;

/* Wide integer type */
typedef unsigned int wint_t;

/* Null pointer */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Offset of member MEMBER in struct TYPE */
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)

#ifdef __cplusplus
#else
static const void* const nullptr = NULL;
#endif

#endif //VESPERAOS_STDDEF_H