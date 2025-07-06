//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H
#include <stdint.h>
#include <stddef.h>
#include "efi_memory.h"

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);
void memset(void* dest, uint8_t val, uint64_t num);
void *memcpy (void *dest, const void *src, size_t len);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
#endif //MEMORY_H