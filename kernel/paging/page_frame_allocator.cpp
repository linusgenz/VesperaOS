#include "page_frame_allocator.h"

#include <vespera/mm/efi_memory.h>
#include <vespera/mm/memory.h>

void PageFrameAllocator::read_efi_memory_map(
    EFI_MEMORY_DESCRIPTOR* m_map, const usize m_map_size, const usize m_map_desc_size
) {
    if (initialized_) return;
    initialized_ = true;

    lock_.init("pfa_lock");

    const usize m_map_entries = m_map_size / m_map_desc_size;

    void* largest_free_mem_seg = nullptr;
    usize largest_free_mem_seg_size = 0;
    void* bitmap_seg = nullptr;
    usize bitmap_seg_size = 0;

    for (usize i = 0; i < m_map_entries; i++) {
        constexpr u64 FOUR_GB = 0x100000000ULL;

        const auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<u64>(m_map) + i * m_map_desc_size);

        if (desc->type != 7) continue;  // EfiConventionalMemory only

        const usize seg_size = desc->num_pages * PAGE_SIZE;

        if (seg_size > largest_free_mem_seg_size) {
            largest_free_mem_seg = reinterpret_cast<void*>(desc->phys_addr);
            largest_free_mem_seg_size = seg_size;
        }

        const bool fits_below_4g = (desc->phys_addr + seg_size) <= FOUR_GB;
        if (fits_below_4g && seg_size > bitmap_seg_size) {
            bitmap_seg = reinterpret_cast<void*>(desc->phys_addr);
            bitmap_seg_size = seg_size;
        }
    }

    if (!bitmap_seg) bitmap_seg = largest_free_mem_seg;

    const u64 memory_size = get_memory_size(m_map, m_map_entries, m_map_desc_size);

    total_memory_ = memory_size;
    free_memory_ = memory_size;
    reserved_memory_ = 0;
    used_memory_ = 0;

    const u64 bitmap_size = memory_size / PAGE_SIZE / 8 + 1;
    init_bitmap(bitmap_size, bitmap_seg);

    reserve_pages(nullptr, memory_size / PAGE_SIZE + 1);

    for (usize i = 0; i < m_map_entries; i++) {
        const auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<u64>(m_map) + i * m_map_desc_size);

        if (desc->type == 7 ||  // EfiConventionalMemory
            desc->type == 1 ||  // EfiLoaderCode
            desc->type == 2 ||  // EfiLoaderData
            desc->type == 3 ||  // EfiBootServicesCode
            desc->type == 4) {  // EfiBootServicesData
            unreserve_pages(desc->phys_addr, desc->num_pages);
        }
    }

    reserve_pages(nullptr, 0x100);  // 0x0..0xFFFFF, low memory
    lock_pages(page_bitmap.buffer, page_bitmap.size / PAGE_SIZE + 1);
}

void PageFrameAllocator::init_bitmap(const usize bitmap_size, void* buffer_address) {
    page_bitmap.size = bitmap_size;
    page_bitmap.buffer = static_cast<u8*>(buffer_address);
    for (usize i = 0; i < bitmap_size; i++) {
        page_bitmap.buffer[i] = 0;
    }
}

void PageFrameAllocator::relocate_bitmap_to_hhdm() {
    // Called once before SMP is up, no lock needed.
    page_bitmap.buffer = reinterpret_cast<u8*>(reinterpret_cast<u64>(page_bitmap.buffer) + g_hhdm_offset);
}

void PageFrameAllocator::lock_page_nolock(void* address) {
    const u64 index = reinterpret_cast<u64>(address) / PAGE_SIZE;
    if (page_bitmap[index]) return;
    if (page_bitmap.set(index, true)) {
        free_memory_ -= PAGE_SIZE;
        used_memory_ += PAGE_SIZE;
    }
}

void PageFrameAllocator::free_page_nolock(const u64 phys_addr) {
    const u64 index = phys_addr / PAGE_SIZE;
    if (!page_bitmap[index]) return;
    if (page_bitmap.set(index, false)) {
        free_memory_ += PAGE_SIZE;
        used_memory_ -= PAGE_SIZE;
        if (page_bitmap_index_ > index) page_bitmap_index_ = index;
    }
}

void PageFrameAllocator::reserve_page_nolock(void* address) {
    const u64 index = reinterpret_cast<u64>(address) / PAGE_SIZE;
    if (page_bitmap[index]) return;
    if (page_bitmap.set(index, true)) {
        free_memory_ -= PAGE_SIZE;
        reserved_memory_ += PAGE_SIZE;
    }
}

void PageFrameAllocator::unreserve_page_nolock(const u64 address) {
    const u64 index = address / PAGE_SIZE;
    if (!page_bitmap[index]) return;
    if (page_bitmap.set(index, false)) {
        free_memory_ += PAGE_SIZE;
        reserved_memory_ -= PAGE_SIZE;
        if (page_bitmap_index_ > index) page_bitmap_index_ = index;
    }
}

u64 PageFrameAllocator::request_page_nolock() {
    const usize max_index = page_bitmap.size * 8;
    for (; page_bitmap_index_ < max_index; page_bitmap_index_++) {
        if (page_bitmap[page_bitmap_index_]) continue;
        lock_page_nolock(reinterpret_cast<void*>(page_bitmap_index_ * PAGE_SIZE));
        return page_bitmap_index_ * PAGE_SIZE;
    }
    return 0;  // OOM
}

u64 PageFrameAllocator::request_page() {
    SpinlockGuardIrq guard(lock_);
    return request_page_nolock();
}

u64 PageFrameAllocator::request_pages(const usize page_count) {
    if (page_count == 0) return 0;
    if (page_count == 1) return request_page();

    // The entire CHECK + LOCK sequence must be atomic.
    // Without a single critical section spanning both loops, a concurrent
    // request_page() on another CPU can steal a page between the check and
    // the lock_page call, giving two callers potentially the same physical page.
    SpinlockGuardIrq guard(lock_);

    const usize max_pages = page_bitmap.size * 8;

    for (u64 i = 0; i < max_pages; i++) {
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
                lock_page_nolock(reinterpret_cast<void*>((i + j) * PAGE_SIZE));
            }
            return i * PAGE_SIZE;
        }
    }

    return 0;  // OOM
}

void PageFrameAllocator::free_page(const u64 phys_addr) {
    SpinlockGuardIrq guard(lock_);
    free_page_nolock(phys_addr);
}

void PageFrameAllocator::free_pages(const u64 address, const usize page_count) {
    SpinlockGuardIrq guard(lock_);
    for (usize t = 0; t < page_count; t++) {
        free_page_nolock(address + t * PAGE_SIZE);
    }
}

void PageFrameAllocator::lock_page(void* address) {
    SpinlockGuardIrq guard(lock_);
    lock_page_nolock(address);
}

void PageFrameAllocator::lock_pages(void* address, const usize page_count) {
    SpinlockGuardIrq guard(lock_);
    for (usize t = 0; t < page_count; t++) {
        lock_page_nolock(reinterpret_cast<void*>(reinterpret_cast<u64>(address) + t * PAGE_SIZE));
    }
}

void PageFrameAllocator::reserve_page(void* address) {
    SpinlockGuardIrq guard(lock_);
    reserve_page_nolock(address);
}

void PageFrameAllocator::reserve_pages(void* address, const usize page_count) {
    SpinlockGuardIrq guard(lock_);
    for (usize t = 0; t < page_count; t++) {
        reserve_page_nolock(reinterpret_cast<void*>(reinterpret_cast<u64>(address) + t * PAGE_SIZE));
    }
}

void PageFrameAllocator::unreserve_page(const u64 address) {
    SpinlockGuardIrq guard(lock_);
    unreserve_page_nolock(address);
}

void PageFrameAllocator::unreserve_pages(const u64 address, const usize page_count) {
    SpinlockGuardIrq guard(lock_);
    for (usize t = 0; t < page_count; t++) {
        unreserve_page_nolock(address + t * PAGE_SIZE);
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
