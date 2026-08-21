// strings.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 18.08.26.
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

#include <ctype.h>
#include <stdlib.h>

#include "string.h"

int strcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        unsigned char c1 = (unsigned char)*s1;
        unsigned char c2 = (unsigned char)*s2;

        c1 = (unsigned char)tolower(c1);
        c2 = (unsigned char)tolower(c2);

        if (c1 != c2)
            return (int)c1 - (int)c2;

        s1++;
        s2++;
    }

    return (int)(unsigned char)tolower((unsigned char)*s1)
        - (int)(unsigned char)tolower((unsigned char)*s2);
}

int strncasecmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && *s2) {
        unsigned char c1 = (unsigned char)*s1;
        unsigned char c2 = (unsigned char)*s2;
        c1 = (unsigned char)tolower(c1);
        c2 = (unsigned char)tolower(c2);
        if (c1 != c2)
            return (int)c1 - (int)c2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(unsigned char)tolower((unsigned char)*s1)
        - (int)(unsigned char)tolower((unsigned char)*s2);
}

void bzero(void* s, size_t n) {
    unsigned char* p = (unsigned char*)s;
    while (n--) {
        *p++ = 0;
    }
}

void bcopy(const void* src, void* dst, size_t n) {
    const unsigned char* s = (const unsigned char*)src;
    unsigned char* d = (unsigned char*)dst;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
}

int bcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

int ffs(int i) {
    if (i == 0)
        return 0;

#if defined(__has_builtin)
#if __has_builtin(__builtin_ctz)
    return __builtin_ctz(i) + 1;
#endif
#elif defined(__GNUC__)
    return __builtin_ctz(i) + 1;
#endif

    int bit = 0;
    if (!i)
        return bit;
    if (!(i & 0xffff)) {
        bit += 16;
        i >>= 16;
    }
    if (!(i & 0xff)) {
        bit += 8;
        i >>= 8;
    }
    if (!(i & 0xf)) {
        bit += 4;
        i >>= 4;
    }
    if (!(i & 0x3)) {
        bit += 2;
        i >>= 2;
    }
    if (!(i & 0x1))
        bit += 1;
    return bit + 1;
}

int ffsll(long long int val) {
    int bit = ffs((unsigned)(val & 0xffffffff));
    if (bit != 0)
        return bit;

    bit = ffs((unsigned)(val >> 32));
    if (bit != 0)
        return 32 + bit;

    return 0;
}

char* index(const char* s, int c) {
    unsigned char ch = (unsigned char)c;
    while (*s) {
        if ((unsigned char)*s == ch)
            return (char*)s;
        s++;
    }
    if (ch == '\0')
        return (char*)s;
    return NULL;
}

char* rindex(const char* s, int c) {
    unsigned char ch = (unsigned char)c;
    const char* last = NULL;
    while (*s) {
        if ((unsigned char)*s == ch)
            last = s;
        s++;
    }
    if (ch == '\0')
        return (char*)s;
    return (char*)last;
}
