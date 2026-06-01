//
// Created by linus on 20.09.24.
//
#ifndef PAGE_FRAME_ALLOCATOR_H
#define PAGE_FRAME_ALLOCATOR_H

#include <vespera/mm/efi_memory.h>
#include <vespera/types.h>

#include "bitmap.h"
#include "vespera/sync/spinlock.h"

class PageFrameAllocator {
   public:
    void read_efi_memory_map(EFI_MEMORY_DESCRIPTOR *m_map, usize m_map_size, usize m_map_desc_size);

    Bitmap page_bitmap{};

    void free_page(u64 phys_addr);

    void free_pages(u64 phys_addr, usize page_count);

    void lock_page(void *address);

    void lock_pages(void *address, usize page_count);

    u64 request_page();

    u64 request_pages(usize page_count);

    [[nodiscard]] u64 get_free_ram() const;

    [[nodiscard]] u64 get_used_ram() const;

    [[nodiscard]] u64 get_reserved_ram() const;

    [[nodiscard]] u64 get_total_ram() const;

    void relocate_bitmap_to_hhdm();

   private:
    void init_bitmap(usize bitmap_size, void *buffer_address);

    void reserve_page(void *address);

    void reserve_pages(void *address, usize page_count);

    void unreserve_page(u64 address);

    void unreserve_pages(u64 address, usize page_count);

    void free_page_nolock(u64 phys_addr);
    void reserve_page_nolock(void *address);
    void unreserve_page_nolock(u64 address);
    u64 request_page_nolock();
    void lock_page_nolock(void *address);

    u64 free_memory_{0};
    u64 reserved_memory_{0};
    u64 used_memory_{0};
    u64 total_memory_{0};
    bool initialized_{false};

    u64 page_bitmap_index_{0};

    Spinlock lock_;
};

#endif  // PAGE_FRAME_ALLOCATOR_H
