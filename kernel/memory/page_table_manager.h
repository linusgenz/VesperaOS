//
// Created by linus on 04.10.24.
//
#ifndef PAGE_TABLE_MANAGER_H
#define PAGE_TABLE_MANAGER_H
#include <cstdint>
#include <cstddef>
#include "../paging/paging.h"

struct PageFlags {
    bool present = false;
    bool read_write = false;
    bool user_super = false;
    bool write_through = false;
    bool cache_disabled = false;
    bool accessed = false;
    bool dirty = false;
    bool huge_page = false;
    bool global = false;
    bool execute_disable = false;
};

class PageTableManager {
    public:
    PageTableManager(PageTable* PML4Address);
    PageTable* PML4;
    void map_memory(void* virtualMemory, void* physicalMemory, uint64_t flags, kprocess_t* proc);

    void set_user_flags(void *virtual_memory, size_t size) const;

    void map_range(void* virt_start, void* phys_start, size_t size, uint64_t flags, kprocess_t* proc);
    void unmap_memory(void* virtualMemory);
    bool is_mapped(void* virtualMemory);
    uint64_t get_physical_address(void* virtualMemory);

    PageFlags get_page_flags(void *virtual_memory);

    PageTable *create_user_pagetable();
};

#endif // PAGE_TABLE_MANAGER_H