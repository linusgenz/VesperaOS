//
// Created by linus on 20.09.24.
//
#ifndef PAGE_FRAME_ALLOCATOR_H
#define PAGE_FRAME_ALLOCATOR_H
#include <cstdint>
#include "bitmap.h"
#include <bootstrap.h>

class PageFrameAllocator {
public:
    PageFrameAllocator() : free_memory(0),
                           reserved_memory(0),
                           used_memory(0),
                           total_memory(0),
                           initialized(false) {
    }

    void read_efi_memory_map(EFI_MEMORY_DESCRIPTOR *mMap, size_t mMapSize, size_t mMapDescSize);

    Bitmap page_bitmap{};

    void free_page(void *address);

    void free_pages(void *address, uint64_t page_count);

    void lock_page(void *address);

    void lock_pages(void *address, uint64_t page_count);

    void *request_page();

    void *request_pages(size_t page_count);

    [[nodiscard]] uint64_t get_free_ram() const;

    [[nodiscard]] uint64_t get_used_ram() const;

    [[nodiscard]] uint64_t get_reserved_ram() const;

    [[nodiscard]] uint64_t get_total_ram() const;

private:
    void init_bitmap(size_t bitmap_size, void *buffer_address);

    void reserve_page(void *address);

    void reserve_pages(void *address, uint64_t page_count);

    void unreserve_page(uint64_t address);

    void unreserve_pages(uint64_t address, uint64_t page_count);

    uint64_t free_memory;
    uint64_t reserved_memory;
    uint64_t used_memory;
    uint64_t total_memory;
    bool initialized;
};

inline PageFrameAllocator* g_allocator = nullptr;

#endif // PAGE_FRAME_ALLOCATOR_H
