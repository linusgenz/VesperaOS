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
#include <stdint.h>

size_t strlen(const char *s) {
    const char *start = s;
    while (*s) {
        ++s;
    }
    return s - start;
}

void strcpy(char *dest, const char *src) {
    if (!dest || !src) return;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char* strtok(char* s, const char delim)
{
    static char* next = NULL;

    if (s != NULL)
        next = s;

    if (next == NULL)
        return NULL;

    char* start = next;

    while (*next != '\0' && *next != delim)
        ++next;

    if (*next == delim)
    {
        *next = '\0';
        ++next;
    }
    else
    {
        next = NULL;
    }

    return start;
}

void strcat(char *dest, const char *src) {
    if (!dest || !src) return;
    while (*dest) dest++;
    strcpy(dest, src);
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

int strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a - *b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n != 0 && *a != '\0' && *b != '\0'  && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : (*a - *b);
}

char *strchr(const char *s, unsigned char c) {
    while (*s) {
        if ((unsigned char)*s == c) {
            return (char *)s;
        }
        ++s;
    }

    if (c == '\0') return (char *)s;
    return NULL;
}

void memset(void* dest, uint8_t c, size_t num) {
    for (uint64_t i = 0; i < num; i++) {
        *(uint8_t*)((uint64_t)dest + i) = c;
    }
}

void *memcpy (void *dest, const void *src, size_t len) {
    char *d = (char*)dest;
    const char *s = (char*)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const uint8_t* a = (const uint8_t*)s1;
    const uint8_t* b = (const uint8_t*)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i])
            return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t len) {
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