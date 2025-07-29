//
// Created by linus on 04.10.24.
//
#ifndef PAGE_TABLE_MANAGER_H
#define PAGE_TABLE_MANAGER_H
#include <stdint.h>
#include "stddef.h"
#include "paging.h"

class PageTableManager {
    public:
    PageTableManager(PageTable* PML4Address);
    PageTable* PML4;
    void map_memory(void* virtualMemory, void* physicalMemory, uint64_t flags = 0);
    void map_range(uintptr_t virt_start, uintptr_t phys_start, size_t size, uint64_t flags);
    void unmap_memory(void* virtualMemory);
    bool is_mapped(void* virtualMemory);
    uint64_t get_physical_address(void* virtualMemory);
};

extern PageTableManager global_page_table_manager ;


#endif // PAGE_TABLE_MANAGER_H