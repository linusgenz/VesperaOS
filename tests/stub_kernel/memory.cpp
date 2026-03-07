// memory.cpp
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

#include <vespera/mm/memory.h>

#include <cstdlib>

namespace kernel::memory {

    void* malloc(size_t size) {
        return std::malloc(size);
    }

    void free(void* p) {
        std::free(p);
    }

    void* alloc_aligned(size_t size, size_t /*alignment*/, size_t /*offset*/) {
        return std::malloc(size);
    }

    void free_aligned(void* p) {
        std::free(p);
    }

    virt_addr_t request_page() {
        return make_virt(std::malloc(PAGE_SIZE));
    }

    virt_addr_t request_pages(size_t n) {
        return make_virt(std::malloc(n * PAGE_SIZE));
    }

    void free_pages(virt_addr_t p, uint64_t /*n*/) {
        std::free(virt_ptr(p));
    }

    void free_page(virt_addr_t p) {
        std::free(virt_ptr(p));
    }

    phys_addr_t request_page_phys() {
        return make_phys(0);
    }

    void map_memory(virt_addr_t virtual_addr, phys_addr_t physical_addr, const uint64_t flags) {}

} // namespace kernel::memory