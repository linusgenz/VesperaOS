//
// Created by linus on 03.10.24.
//
#ifndef PAGING_H
#define PAGING_H

#include <vespera/mm/addr.h>
#include <vespera/mm/memory.h>

struct PageDirectoryEntry {
    uint64_t value;
    void set_flag(PtFlag flag, bool enabled);
    [[nodiscard]] bool get_flag(PtFlag flag) const;
    void set_address(phys_addr_t phys_addr);
    [[nodiscard]] phys_addr_t get_address() const;
};

struct PageTable {
    PageDirectoryEntry entries[512];
} __attribute__((aligned(0x1000)));

#endif  // PAGING_H