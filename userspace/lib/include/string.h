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

#include <stdint.h>
#include <stddef.h>

size_t strlen(const char *s);

void strcpy(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

void strcat(char *dest, const char *src);

char* strncat(char* dest, const char* src, size_t max);

int strcmp(const char *a, const char *b);

void memset(void* dest, uint8_t val, uint64_t num);

void *memcpy (void *dest, const void *src, size_t len);

int memcmp(const void* ptr1, const void* ptr2, size_t num);

#endif //VESPERAOS_STRING_H