//
// Created by linus on 03.10.24.
//
#ifndef PAGING_H
#define PAGING_H

#include <cstdint>

enum PT_Flag
{
    Present = 0,
    ReadWrite = 1,
    UserSuper = 2,
    WriteThrough = 3,
    CacheDisabled = 4,
    Accessed = 5,
    Dirty = 6,
    LargerPages = 7,
    Global = 8,
    Custom0 = 9,
    Custom1 = 10,
    Custom2 = 11,
    NX = 63 // only if supported
};

struct PageDirectoryEntry {
    uint64_t Value;
    void set_flag(PT_Flag flag, bool enabled);
    bool get_flag(PT_Flag flag) const;
    void set_address(uint64_t address);
    uint64_t get_address();
};

struct PageTable { 
    PageDirectoryEntry entries [512];
}__attribute__((aligned(0x1000)));

#endif // PAGING_H