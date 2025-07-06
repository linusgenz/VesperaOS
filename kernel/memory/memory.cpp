#include "../include/memory.h"

#include "../include/basic_renderer.h"

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize) {
    uint64_t memory_size_bytes = 0; // static
    // if (memory_size_bytes > 0) return memory_size_bytes;

    for (int i = 0; i < mMapEntries; i++) {
        const auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR *>(reinterpret_cast<uint64_t>(mMap) + (i * mMapDescSize));
        if (desc->type != 7) continue;
        memory_size_bytes += desc->num_pages * 4096;
    }

    return memory_size_bytes;
}

void memset(void* dest, uint8_t val, uint64_t num) {
    for (uint64_t i = 0; i < num; i++) {
        *(uint8_t*)((uint64_t)dest + i) = val;
    }
}

void *memcpy (void *dest, const void *src, size_t len) {
    char *d = (char*)dest;
    const char *s = (char*)src;
    while (len--)
        *d++ = *s++;
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const uint8_t* a = (const uint8_t*)ptr1;
    const uint8_t* b = (const uint8_t*)ptr2;
    for (size_t i = 0; i < num; i++) {
        if (a[i] != b[i])
            return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}