/**
 * @file memory.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 08.01.26.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef VESPERAOS_MEMSET_H
#define VESPERAOS_MEMSET_H

#include <cstddef>

typedef unsigned long EFI_PHYSICAL_ADDRESS;
typedef unsigned long EFI_VIRTUAL_ADDRESS;

struct EFI_MEMORY_DESCRIPTOR {
    uint32_t type;
    uint32_t pad;
    EFI_PHYSICAL_ADDRESS phys_addr;
    EFI_VIRTUAL_ADDRESS virt_addr;
    uint64_t num_pages;
    uint64_t attribs;
};

#define PAGE_SIZE 0x1000

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);

void memset(void* dest, uint8_t val, uint64_t num);

void* memcpy(void* dest, const void* src, size_t len);

int memcmp(const void* ptr1, const void* ptr2, size_t num);

void* memmove(void* dest, const void* src, size_t len);

#endif //VESPERAOS_MEMSET_H