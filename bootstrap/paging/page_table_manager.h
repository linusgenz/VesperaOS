//
// Created by linus on 04.10.24.
//
#ifndef PAGE_TABLE_MANAGER_H
#define PAGE_TABLE_MANAGER_H

#include <cstdint>
#include <cstddef>
#include "paging.h"

class PageTableManager
{
public:
    PageTableManager(PageTable* PML4Address);
    PageTable* PML4;
    void map_memory(void* virtual_memory, void* physical_memory, uint64_t flags) const;
    void map_kernel_page(void* phys_addr, uint64_t flags) const;

    void set_user_flags(void* virtual_memory, size_t size) const;

    void map_range(void* virt_start, void* phys_start, size_t size, uint64_t flags) const;

    void unmap_range(void* virt_start, size_t size) const;

    void unmap_memory(void* virtual_memory) const;
    bool is_mapped(void* virtual_memory) const;
    void* get_physical_address(void* virtual_memory) const;
};

inline PageTableManager* g_ptm = nullptr;

#endif // PAGE_TABLE_MANAGER_H
