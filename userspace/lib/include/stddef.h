// stddef.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 02.12.25.
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

#ifndef _STDDEF_H
#define _STDDEF_H

/* ptrdiff_t */
typedef __PTRDIFF_TYPE__ ptrdiff_t;

/* size_t */
typedef __SIZE_TYPE__ size_t;

/* wchar_t */
#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

/* wint_t */
typedef __WINT_TYPE__ wint_t;

/* NULL */
#ifndef NULL
#ifdef __cplusplus
#define NULL nullptr
#else
#define NULL ((void*)0)
#endif
#endif

/* offsetof */
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)

#endif