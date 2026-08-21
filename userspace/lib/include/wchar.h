// wchar.h
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
#ifndef _WCHAR_H
#define _WCHAR_H

#include <bits/alltypes.h>

#ifndef __cplusplus
typedef __WCHAR_TYPE__ wchar_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WINT_T
#define _WINT_T
    typedef unsigned int wint_t;
#endif

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

    size_t  wcslen(const wchar_t* str);
    wchar_t* wcscpy(wchar_t* dest, const wchar_t* src);
    int     wcscmp(const wchar_t* s1, const wchar_t* s2);
    wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n);
    int     wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n);

    wchar_t* wmemchr(const wchar_t* ptr, wchar_t ch, size_t count);
    int     wmemcmp(const wchar_t* ptr1, const wchar_t* ptr2, size_t count);
    wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t count);
    wchar_t* wmemmove(wchar_t* dest, const wchar_t* src, size_t count);
    wchar_t* wmemset(wchar_t* dest, wchar_t ch, size_t count);

#ifdef __cplusplus
}
#endif

#endif //VESPERAOS_WCHAR_H