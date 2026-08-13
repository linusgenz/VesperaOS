// mman.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 27.09.25.
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sysstd.h>

void* mmap(void* addr, size_t length, uint64_t prot, uint64_t flags, uint64_t handle, size_t offset) {
    int64_t ret = sys_mmap((uint64_t)addr, length, prot, flags, handle, offset);
    if (ret < 0) {
        errno = -ret;
        return MAP_FAILED;
    }
    return (void*)ret;
}

int munmap(void* addr, size_t length) {
    const int64_t ret = sys_munmap((uint64_t)addr, length, 0, 0, 0);
    if (ret < 0) {
        errno = -(ret);
        return -1;
    }
    return 0;
}

int mprotect(void* addr, size_t length, int prot) {
    const int64_t ret = sys_mprotect((uint64_t)addr, length, prot, 0, 0, 0);
    if (ret < 0) {
        errno = -(ret);
        return -1;
    }
    return 0;
}