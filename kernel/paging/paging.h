//
// Created by linus on 03.10.24.
//
#ifndef PAGING_H
#define PAGING_H

#include <kernel/memory.h>

struct PageDirectoryEntry {
    uint64_t Value;
    void set_flag(PT_Flag flag, bool enabled);
    bool get_flag(PT_Flag flag) const;
    void set_address(uint64_t address);
    uint64_t get_address() const;
};

struct PageTable {
    PageDirectoryEntry entries[512];
} __attribute__((aligned(0x1000)));

#endif  // PAGING_H