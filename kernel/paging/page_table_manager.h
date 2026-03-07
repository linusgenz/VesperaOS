//
// Created by linus on 04.10.24.
//
#ifndef PAGE_TABLE_MANAGER_H
#define PAGE_TABLE_MANAGER_H

#include <vespera/mm/addr.h>
#include "paging.h"

class PageTableManager {
   public:
    PageTableManager(PageTable* pml4_address);
    PageTable* pml4;
    void map_memory(virt_addr_t virtual_memory, phys_addr_t physical_memory, uint64_t flags) const;
  //  void map_kernel_page(void* phys_addr, uint64_t flags) const;

//    void set_user_flags(void* virtual_memory, size_t size) const;

    void map_range(virt_addr_t virt_start, phys_addr_t phys_start, size_t size, uint64_t flags) const;

    void unmap_range(virt_addr_t virt_start, size_t size) const;

    void unmap_memory(virt_addr_t virt_addr) const;
    [[nodiscard]] bool is_mapped(virt_addr_t virt_addr) const;
    phys_addr_t get_physical_address(virt_addr_t virt_addr) const;
};

#endif  // PAGE_TABLE_MANAGER_H
