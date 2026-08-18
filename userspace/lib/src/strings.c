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

int strcasecmp(const char *s1, const char *s2) {
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

int strncasecmp(const char *s1, const char *s2, size_t n) {
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

void bzero(void *s, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = 0;
    }
}

void bcopy(const void *src, void *dst, size_t n) {
    const unsigned char *s = (const unsigned char *)src;
    unsigned char *d = (unsigned char *)dst;

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

    unsigned int u = (unsigned int)i;
    int pos = 1;
    while ((u & 1u) == 0) {
        u >>= 1;
        pos++;
    }
    return pos;
}

char *index(const char *s, int c) {
    unsigned char ch = (unsigned char)c;
    while (*s) {
        if ((unsigned char)*s == ch)
            return (char *)s;
        s++;
    }
    if (ch == '\0')
        return (char *)s;
    return NULL;
}

char *rindex(const char *s, int c) {
    unsigned char ch = (unsigned char)c;
    const char *last = NULL;
    while (*s) {
        if ((unsigned char)*s == ch)
            last = s;
        s++;
    }
    if (ch == '\0')
        return (char *)s;
    return (char *)last;
}
