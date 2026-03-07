// memory.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.03.26.
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
#pragma once
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vespera/mm/addr.h>

#define PAGE_SIZE 4096

inline void memset(virt_addr_t dest, uint8_t val, uint64_t num) {
    memset(virt_ptr(dest), val, num);
}

namespace kernel::memory {

    void* malloc(size_t size);
    void free(void* p);

    void* alloc_aligned(size_t size, size_t alignment, size_t offset = 0);
    void free_aligned(void* p);

    virt_addr_t request_page();
    virt_addr_t request_pages(size_t n);
    void free_pages(virt_addr_t p, uint64_t n);
    void free_page(virt_addr_t p);
    phys_addr_t request_page_phys();

    void map_memory(virt_addr_t virtual_addr, phys_addr_t physical_addr, const uint64_t flags = 0);
}  // namespace kernel::memory