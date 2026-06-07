// wchar.c
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

#include <stdlib.h>

size_t wcslen(const wchar_t* str) {
    size_t len = 0;
    while (str[len] != 0) {
        len++;
    }
    return len;
}

int wmemcmp(const wchar_t* ptr1, const wchar_t* ptr2, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (ptr1[i] < ptr2[i]) return -1;
        if (ptr1[i] > ptr2[i]) return 1;
    }
    return 0;
}

wchar_t* wmemcpy(wchar_t* dest, const wchar_t* src, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = src[i];
    }
    return dest;
}

wchar_t* wmemset(wchar_t* dest, wchar_t ch, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dest[i] = ch;
    }
    return dest;
}