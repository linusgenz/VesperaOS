#include "page_frame_allocator.h"

#include <vespera/mm/efi_memory.h>
#include <vespera/mm/memory.h>

PageFrameAllocator global_allocator;

void PageFrameAllocator::read_efi_memory_map(EFI_MEMORY_DESCRIPTOR* m_map, usize m_map_size, usize m_map_desc_size) {
     if (initialized_) return;
    initialized_ = true;

    const usize m_map_entries = m_map_size / m_map_desc_size;

    void* largest_free_mem_seg = nullptr;
    usize largest_free_mem_seg_size = 0;

    for (usize i = 0; i < m_map_entries; i++) {
        if (const auto* desc =
                reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<u64>(m_map) + (i * m_map_desc_size));
            desc->type == 7) {  // type = EfiConventionalMemory
            if (desc->num_pages * 4096 > largest_free_mem_seg_size) {
                largest_free_mem_seg = reinterpret_cast<void*>(desc->phys_addr);
                largest_free_mem_seg_size = desc->num_pages * 4096;
            }
        }
    }
    const u64 memory_size = get_memory_size(m_map, m_map_entries, m_map_desc_size);

    total_memory_ = memory_size;
    free_memory_ = memory_size;
    reserved_memory_ = 0;
    used_memory_ = 0;
    const u64 bitmap_size = memory_size / 4096 / 8 + 1;

    init_bitmap(bitmap_size, largest_free_mem_seg);

    reserve_pages(nullptr, memory_size / 4096 + 1);
    for (usize i = 0; i < m_map_entries; i++) {
        const auto* desc =
            reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<u64>(m_map) + (i * m_map_desc_size));

        if (desc->type == 7 ||  // EfiConventionalMemory
            desc->type == 1 ||  // EfiLoaderCode
            desc->type == 2 ||  // EfiLoaderData
            desc->type == 3 ||  // EfiBootServicesCode
            desc->type == 4) {  // EfiBootServicesData
            unreserve_pages(desc->phys_addr, desc->num_pages);
            }
    }

    reserve_pages(nullptr, 0x100);  // reserve between 0 and 0x100000
    lock_pages(page_bitmap.buffer, page_bitmap.size / 4096 + 1);
}

void PageFrameAllocator::init_bitmap(const usize bitmap_size, void* buffer_address) {
    page_bitmap.size = bitmap_size;
    page_bitmap.buffer = static_cast<u8*>(buffer_address);
    for (usize i = 0; i < bitmap_size; i++) {
        *(page_bitmap.buffer + i) = 0;
    }
}

void PageFrameAllocator::relocate_bitmap_to_hhdm() {
    page_bitmap.buffer = reinterpret_cast<u8*>(reinterpret_cast<u64>(page_bitmap.buffer) + g_hhdm_offset);
}

u64 page_bitmap_index = 0;
u64 PageFrameAllocator::request_page() {
    for (; page_bitmap_index < page_bitmap.size * 8; page_bitmap_index++) {
        if (page_bitmap[page_bitmap_index] == true) continue;
        lock_page(reinterpret_cast<void*>(page_bitmap_index * 4096));
        return (page_bitmap_index * 4096);
    }

    return 0;  // Page Frame Swap to file
}

u64 PageFrameAllocator::request_pages(const usize page_count) {
    if (page_count == 1) return request_page();

    const usize max_pages = page_bitmap.size * 8;

    for (u64 i = 0; i < max_pages; i++) {
        // check for continuous memory
        bool block_free = true;
        for (usize j = 0; j < page_count; j++) {
            if ((i + j) >= max_pages || page_bitmap[i + j]) {
                block_free = false;
                i += j;
                break;
            }
        }

        if (block_free) {
            for (usize j = 0; j < page_count; j++) {
                lock_page(reinterpret_cast<void*>((i + j) * 4096));
            }
            return (i * 4096);
        }
    }

    return 0;  // nothing found
}

void PageFrameAllocator::free_page(u64 phys_addr) {
    u64 index = phys_addr / 4096;
    if (page_bitmap[index] == false) return;
    if (page_bitmap.set(index, false)) {
        free_memory_ += 4096;
        used_memory_ -= 4096;
        if (page_bitmap_index > index) page_bitmap_index = index;
    }
}

void PageFrameAllocator::free_pages(u64 address, const usize page_count) {
    for (usize t = 0; t < page_count; t++) {
        free_page((address + (t * 4096)));
    }
}

void PageFrameAllocator::lock_page(void* address) {
    u64 index = reinterpret_cast<u64>(address) / 4096;
    if (page_bitmap[index] == true) return;
    if (page_bitmap.set(index, true)) {
        free_memory_ -= 4096;
        used_memory_ += 4096;
    }
}

void PageFrameAllocator::lock_pages(void* address, const usize page_count) {
    for (usize t = 0; t < page_count; t++) {
        lock_page(reinterpret_cast<void*>(reinterpret_cast<u64>(address) + (t * 4096)));
    }
}

void PageFrameAllocator::unreserve_page(u64 address) {
    const u64 index = address / 4096;
    if (page_bitmap[index] == false) return;
    if (page_bitmap.set(index, false)) {
        free_memory_ += 4096;
        reserved_memory_ -= 4096;
        if (page_bitmap_index > index) page_bitmap_index = index;
    }
}

void PageFrameAllocator::unreserve_pages(u64 address, const usize page_count) {
    for (usize t = 0; t < page_count; t++) {
        unreserve_page(address + (t * 4096));
    }
}

void PageFrameAllocator::reserve_page(void* address) {
    u64 index = reinterpret_cast<u64>(address) / 4096;
    if (page_bitmap[index] == true) return;
    if (page_bitmap.set(index, true)) {
        free_memory_ -= 4096;
        reserved_memory_ += 4096;
    }
}

void PageFrameAllocator::reserve_pages(void* address, const usize page_count) {
    for (usize t = 0; t < page_count; t++) {
        reserve_page(reinterpret_cast<void*>(reinterpret_cast<u64>(address) + (t * 4096)));
    }
}

u64 PageFrameAllocator::get_free_ram() const {
    return free_memory_;
}
u64 PageFrameAllocator::get_used_ram() const {
    return used_memory_;
}
u64 PageFrameAllocator::get_reserved_ram() const {
    return reserved_memory_;
}

u64 PageFrameAllocator::get_total_ram() const {
    return total_memory_;
}
