// string.h
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

#ifndef VESPERAOS_STRING_H
#define VESPERAOS_STRING_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Return the length of S.
 */
size_t strlen(const char *s);

size_t strnlen(const char* s, size_t maxlen);

/**
 * @brief Copy SRC to DEST.
 */
char* strcpy(char *dest, const char *src);

/**
 * @brief Copy no more than N characters of SRC to DEST.
 */
char *strncpy(char *dest, const char *src, size_t n);

/**
 * @brief Append SRC onto DEST.
 */
char* strcat(char *dest, const char *src);

/**
 * @brief Append no more than N characters from SRC onto DEST.
 */
char *strncat(char *dest, const char *src, size_t max);

/**
 * @brief Compare A and B.
 */
int strcmp(const char *a, const char *b);

/**
 * @brief Compare N characters of A and B.
 */
int strncmp(const char *a, const char *b, size_t n);

/**
 * @brief Stub, same as strcmp
 */
int strcoll(const char *s1, const char *s2);

/**
 * @brief Find the first occurrence of C in S.
 */
char *strchr(const char *s, int c);

char *strrchr(const char *s, int c);

/**
 * @brief Find the first occurrence of NEEDLE in HAYSTACK.
 */
char *strstr(const char *haystack, const char *needle);
/**
 * @brief Set N bytes of DEST to C.
 */
void* memset(void *dest, uint8_t c, size_t n);

/**
 * @brief Copy N bytes of SRC to DEST.
 */
void *memcpy(void *dest, const void *src, size_t len);

/**
 * @brief Compare N bytes of S1 and S2.
 */
int memcmp(const void *s1, const void *s2, size_t n);

/**
 * @brief Copy N bytes of SRC to DEST, guaranteeing
 * correct behavior for overlapping strings.
 */
void *memmove(void *dest, const void *src, size_t len);

/**
 * @brief Split a string into tokens using a delimiter.
 */
char *strtok(char *s, char delim);

/**
 * @brief Return the length of the maximum initial segment
   of S which contains only characters in ACCEPT.
 */
size_t strspn(const char *s, const char *accept);

/**
 * @brief Return the length of the initial segment of S which
   consists entirely of characters not in REJECT.
*/
size_t strcspn(const char *s, const char *reject);

/**
 * @brief Find the first occurrence in S of any character in ACCEPT.
 */
char *strpbrk(const char *s, const char *accept);

void *memchr(const void *src, int c, size_t n);

#endif  // VESPERAOS_STRING_H