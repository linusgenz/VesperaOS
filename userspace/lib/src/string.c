// string.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 22.09.25.
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

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

size_t strlen(const char* s) {
    const char* start = s;
    while (*s) {
        ++s;
    }
    return s - start;
}

size_t strnlen(const char* s, size_t maxlen) {
    const char* start = s;
    while (maxlen-- && *s) ++s;
    return s - start;
}

char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i = 0;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d) d++;
    while (*src) *d++ = *src++;
    *d = '\0';
    return dest;
}

char* strncat(char* dest, const char* src, size_t max) {
    if (!dest || !src || max == 0) return dest;

    size_t dlen = 0;
    while (dlen < max && dest[dlen] != '\0') {
        dlen++;
    }

    if (dlen == max) return dest;

    size_t i = 0;
    while (i + dlen < max - 1 && src[i] != '\0') {
        dest[dlen + i] = src[i];
        i++;
    }

    dest[dlen + i] = '\0';
    return dest;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n != 0 && *a != '\0' && *b != '\0' && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : (*a - *b);
}

int strcoll(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

char* strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (*needle == '\0') return (char*)haystack;

    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') return (char*)haystack;
    }
    return NULL;
}

static char* strchrnul_c(const char* s, int c) {
    unsigned char ch = (unsigned char)c;

    while (*s && (unsigned char)*s != ch) {
        ++s;
    }

    return (char*)s;
}

size_t strcspn(const char* s, const char* reject) {
    /* Fast path: leeres oder 1-Zeichen-Set */
    if (reject[0] == '\0') {
        const char* p = s;
        while (*p) ++p;
        return (size_t)(p - s);
    }

    if (reject[1] == '\0') {
        return (size_t)(strchrnul_c(s, reject[0]) - s);
    }

    /* Allgemeiner Fall: Lookup-Table (256 Bytes) */
    uint8_t table[256] = {0};

    for (const unsigned char* r = (const unsigned char*)reject; *r; ++r) {
        table[*r] = 1;
    }

    const unsigned char* p = (const unsigned char*)s;
    while (*p && !table[*p]) {
        ++p;
    }

    return (size_t)(p - (const unsigned char*)s);
}

size_t strspn(const char* s, const char* accept) {
    if (accept[0] == '\0') {
        return 0;
    }

    if (accept[1] == '\0') {
        const char* p = s;
        while (*p == accept[0]) {
            ++p;
        }
        return (size_t)(p - s);
    }

    uint8_t table[256] = {0};

    for (const unsigned char* a = (const unsigned char*)accept; *a; ++a) {
        table[*a] = 1;
    }

    const unsigned char* p = (const unsigned char*)s;
    while (*p && table[*p]) {
        ++p;
    }

    return (size_t)(p - (const unsigned char*)s);
}

char* strpbrk(const char* s, const char* accept) {
    s += strcspn(s, accept);
    return *s ? (char*)s : NULL;
}

static int has_zero(size_t x) {
    size_t ones = (size_t)-1 / 0xFF;
    size_t highs = ones * 0x80;
    return ((x - ones) & ~x & highs) != 0;
}

void* memchr(const void* src, int c, size_t n) {
    const unsigned char* s = (const unsigned char*)src;
    unsigned char target = (unsigned char)c;

    while (((uintptr_t)s % sizeof(size_t)) != 0 && n > 0) {
        if (*s == target) return (void*)s;
        s++;
        n--;
    }

    if (n >= sizeof(size_t)) {
        size_t repeated = 0;
        for (size_t i = 0; i < sizeof(size_t); i++) {
            repeated = (repeated << 8) | target;
        }

        const size_t* w = (const size_t*)s;

        while (n >= sizeof(size_t)) {
            size_t x = *w ^ repeated;

            if (has_zero(x)) {
                break;
            }

            w++;
            n -= sizeof(size_t);
        }

        s = (const unsigned char*)w;
    }

    while (n > 0) {
        if (*s == target) return (void*)s;
        s++;
        n--;
    }

    return NULL;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if ((unsigned char)*s == c) {
            return (char*)s;
        }
        ++s;
    }

    if (c == '\0') return (char*)s;
    return NULL;
}

char* strrchr(const char* s, int c) {
    return memchr(s, c, strlen(s) + 1);
}


char* strtok_r(char* str, const char* delim, char** saveptr) {
    char* next = NULL;

    if (str == NULL) {
        next = *saveptr;
    } else {
        next = str;
    }

    if (next == NULL) {
        return NULL;
    }

    while (*next != '\0' && strchr(delim, *next)) {
        ++next;
    }

    if (*next == '\0') {
        *saveptr = NULL;
        return NULL;
    }

    char* start = next;

    while (*next != '\0' && !strchr(delim, *next)) {
        ++next;
    }

    if (*next != '\0') {
        *next = '\0';
        *saveptr = next + 1;
    } else {
        *saveptr = NULL;
    }

    return start;
}

char* strtok(char* s, const char* delim) {
    _Thread_local static char* saveptr = NULL;
    return strtok_r(s, delim, &saveptr);
}

void* memset(void* dest, int c, size_t num) {
    uint8_t* d = (uint8_t*)dest;

    for (size_t i = 0; i < num; i++) {
        d[i] = (uint8_t)c;
    }

    return dest;
}

void* memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (char*)src;
    while (len--) *d++ = *s++;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t len) {
    if (len == 0 || dest == src) return dest;

    char* d = (char*)(dest);
    const char* s = src;
    if (d < s) {
        while (len--) *d++ = *s++;
    }
    else {
        char* lasts = (char*)(s + (len - 1));
        char* lastd = d + (len - 1);
        while (len--) *lastd-- = *lasts--;
    }
    return dest;
}

int memmove_safe(void* dest, size_t dest_len,
                 const void* src, size_t count) {
    if (!dest || !src) return -1;
    if (count > dest_len) return -2;
    if (count == 0 || dest == src) return 0;

    char* d = (char*)dest;
    const char* s = (const char*)src;

    if (d < s) {
        while (count--) *d++ = *s++;
    }
    else {
        d += count;
        s += count;
        while (count--) *--d = *--s;
    }

    return 0;
}

size_t strlcpy(char* dest, const char* src, size_t size) {
    size_t src_len = strlen(src);
    if (size) {
        size_t min_len = (src_len >= size) ? size - 1 : src_len;
        memcpy(dest, src, min_len);
        dest[min_len] = '\0';
    }
    return src_len;
}

char *strndup(const char *s, size_t n) {
    if (!s) return NULL;

    size_t len = strnlen(s, n);
    char *dst = malloc(len + 1);

    if (dst == NULL) {
        return NULL;
    }

    memcpy(dst, s, len);
    dst[len] = '\0';

    return dst;
}

char *strdup(const char *s) {
    return strndup(s, (size_t)-1);
}