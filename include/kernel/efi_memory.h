//
// Created by linus on 18.09.24.
//

#ifndef EFI_MEMORY_H
#define EFI_MEMORY_H
#include <stdint.h>

typedef unsigned long efi_physical_address_t;
typedef unsigned long efi_virtual_address_t;

struct EFI_MEMORY_DESCRIPTOR {
    uint32_t type;
    efi_physical_address_t phys_addr;
    efi_virtual_address_t virt_addr;
    uint64_t num_pages;
    uint64_t attribs;
};

extern const char* efi_memory_type_strings[];
#endif //EFI_MEMORY_H