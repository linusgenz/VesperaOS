// strings.h
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
#ifndef _STRINGS_H
#define _STRINGS_H

#include <stdlib.h>

int strcasecmp(const char* s1, const char* s2);
int strncasecmp(const char* s1, const char* s2, size_t n);

void bzero(void* s, size_t n);
void bcopy(const void* src, void* dst, size_t n);
int bcmp(const void* s1, const void* s2, size_t n);

int ffs(int i);
int ffsll(long long int val);
char* index(const char* s, int c);
char* rindex(const char* s, int c);

#ifdef __cplusplus
} // extern "C"
#endif


#endif // _STRINGS_H
