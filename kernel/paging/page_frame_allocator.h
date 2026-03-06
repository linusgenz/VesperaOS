//
// Created by linus on 20.09.24.
//
#ifndef PAGE_FRAME_ALLOCATOR_H
#define PAGE_FRAME_ALLOCATOR_H
#include <kernel/efi_memory.h>

#include "bitmap.h"
#include <stdint.h>
#include <stddef.h>

class PageFrameAllocator {
   public:

    void read_efi_memory_map(EFI_MEMORY_DESCRIPTOR *m_map, size_t m_map_size, size_t m_map_desc_size);

    Bitmap page_bitmap{};

    void free_page(uint64_t phys_addr);

    void free_pages(uint64_t phys_addr, size_t page_count);

    void lock_page(void *address);

    void lock_pages(void *address, size_t page_count);

    uint64_t request_page();

    uint64_t request_pages(size_t page_count);

    [[nodiscard]] uint64_t get_free_ram() const;

    [[nodiscard]] uint64_t get_used_ram() const;

    [[nodiscard]] uint64_t get_reserved_ram() const;

    [[nodiscard]] uint64_t get_total_ram() const;

    void relocate_bitmap_to_hhdm();

   private:
    void init_bitmap(size_t bitmap_size, void *buffer_address);

    void reserve_page(void *address);

    void reserve_pages(void *address, size_t page_count);

    void unreserve_page(uint64_t address);

    void unreserve_pages(uint64_t address, size_t page_count);

    uint64_t free_memory_{0};
    uint64_t reserved_memory_{0};
    uint64_t used_memory_{0};
    uint64_t total_memory_{0};
    bool initialized_{false};
};

#endif  // PAGE_FRAME_ALLOCATOR_H
