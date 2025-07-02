//
// Created by linus on 18.09.24.
//

#ifndef EFI_MEMORY_H
#define EFI_MEMORY_H
#include <stdint.h>

typedef unsigned long EFI_PHYSICAL_ADDRESS;
typedef unsigned long EFI_VIRTUAL_ADDRESS;

struct EFI_MEMORY_DESCRIPTOR {
    uint32_t type;
    EFI_PHYSICAL_ADDRESS phys_addr;
    EFI_VIRTUAL_ADDRESS virt_addr;
    uint64_t num_pages;
    uint64_t attribs;
};

extern const char* EFI_MEMORY_TYPE_STRINGS[];
#endif //EFI_MEMORY_H