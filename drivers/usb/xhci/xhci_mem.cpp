// xhci_mem.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 21.07.25.
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

#include <vespera/log.h>
#include <vespera/mm/memory.h>

uintptr_t xhci_map_mmio(uint64_t pci_bar_address, uint32_t bar_size) {
    phys_addr_t mmio_phys = make_phys(pci_bar_address);
    virt_addr_t mmio_virt = phys_to_virt(mmio_phys);
    kernel::memory::map_range(mmio_virt, mmio_phys, bar_size, (1ULL << CacheDisabled));

    return virt_raw(mmio_virt);
}


void *alloc_xhci_memory(size_t size, size_t alignment, size_t boundary) {
    if (size == 0 || alignment == 0 || boundary == 0) {
        Log::error("Invalid memory alignment");
    }

    void *memblock = kernel::memory::alloc_aligned(size, alignment, boundary);

    if (!memblock) {
        Log::error("Failed to allocate memory");
        while (true);
    }

    memset(memblock, 0, size);
    return memblock;
}

void free_xhci_memory(void *ptr) {
    kernel::memory::free_aligned(ptr);
}

uintptr_t xhci_get_physical_addr(void *virt) {
    return kernel::memory::get_physical_address(make_virt(virt)).raw;
}
