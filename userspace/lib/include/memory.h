// memory.h
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

#ifndef VESPERAOS_MEMORY_H
#define VESPERAOS_MEMORY_H
#include <stddef.h>


/**
 * @brief Header for a heap segment.
 *
 * Stored in front of each allocated or free block in the user heap.
 */
typedef struct heap_seg {
    size_t length;
    int free;
    uint32_t magic;
    struct heap_seg* next;
    struct heap_seg* prev;
} heap_seg;

typedef struct large_seg {
    void* addr;
    size_t size;
    struct large_seg* next;
} large_seg;

#endif //VESPERAOS_MEMORY_H
