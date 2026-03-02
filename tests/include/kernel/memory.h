// mock_kernel_memory.h
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

#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <new>

#define PAGE_SIZE 4096

namespace kernel::memory {
    inline void* malloc(size_t size)  { return std::malloc(size); }
    inline void  free(void* p)        { std::free(p); }
    inline void* alloc_aligned(size_t s, size_t, size_t = 0) { return std::malloc(s); }
    inline void  free_aligned(void* p) { std::free(p); }

    inline void* request_page()  { return std::malloc(4096); }
    inline void* request_pages(size_t n) { return std::malloc(n * 4096); }
    inline void  free_pages(const void* p, uint64_t) { std::free(const_cast<void*>(p)); }
    inline void  free_page(const void* p) { std::free(const_cast<void*>(p)); }
}