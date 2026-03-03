//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H

#include <boot.h>

#include "addr.h"
#include "efi_memory.h"
#include <cstddef>

#define PAGE_SIZE 4096
inline uint64_t g_hhdm_offset = 0;
inline uint64_t g_kernel_phys_base = 0;
inline uint64_t g_kernel_virt_base = 0;

struct PageTable;

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, size_t mMapEntries, size_t mMapDescSize);

void memset(void* dest, uint8_t val, uint64_t num);

inline void memset(virt_addr_t dest, uint8_t val, uint64_t num) {
    memset(virt_ptr(dest), val, num);
}

void* memcpy(void* dest, const void* src, size_t len);

int memcmp(const void* ptr1, const void* ptr2, size_t num);

void* memmove(void* dest, const void* src, size_t len);

virt_addr_t phys_to_virt(phys_addr_t addr);
phys_addr_t virt_to_phys(virt_addr_t addr);

enum PT_Flag {
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
    NX = 63  // only if supported
};

namespace kernel::memory {
    void initialize_memory(BootInfo* bootInfo);

    // Page Table Management
    void initialize_page_table_manager(BootInfo* bootInfo);

    void map_memory(virt_addr_t virtual_addr, phys_addr_t physical_addr, uint64_t flags = 0);

    //void set_user_flags(void* virtual_memory, size_t size);

    void map_range(virt_addr_t virt_start, phys_addr_t phys_start, size_t size, uint64_t flags = 0);

    void unmap_range(virt_addr_t virt_start, size_t size);

    void unmap_memory(virt_addr_t virtual_addr);

    bool is_mapped(virt_addr_t virtual_addr);

    uintptr_t get_pagetable_address();

    phys_addr_t get_physical_address(virt_addr_t virtual_addr);

    // Page Frame Allocator
    void initialize_page_frame_allocator(void* efi_memory_map, size_t map_size, size_t desc_size);

    void free_page(virt_addr_t virtual_addr);

    void free_page_phys(phys_addr_t phys_addr);

    void free_pages(virt_addr_t virtual_addr, uint64_t page_count);

    void free_pages_phys(phys_addr_t phys_addr, uint64_t page_count);

    void lock_page(phys_addr_t phys_addr);

    void lock_pages(phys_addr_t phys_addr, uint64_t page_count);

    void relocate_bitmap_to_hhdm();

    [[nodiscard]] virt_addr_t request_page();

    [[nodiscard]] phys_addr_t request_page_phys();

    [[nodiscard]] virt_addr_t request_pages(size_t pageCount);

    [[nodiscard]] phys_addr_t request_pages_phys(size_t pageCount);

    [[nodiscard]] uint64_t get_free_ram();

    [[nodiscard]] uint64_t get_used_ram();

    [[nodiscard]] uint64_t get_reserved_ram();

    [[nodiscard]] uint64_t get_total_ram();

    // Heap Allocator
    void initialize_heap(virt_addr_t heap_start, size_t page_count);

    void* alloc_aligned(size_t size, size_t alignment, size_t boundary = 0);

    void free_aligned(void* aligned_ptr);

    [[nodiscard]] void* malloc(size_t size);

    void free(void* addr);

    void* realloc(void* old_ptr, size_t old_size, size_t new_size);

    void print_heap_stats();
}  // namespace kernel::memory

void* operator new(size_t size);
void* operator new[](size_t size);
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}
void operator delete(void* p) noexcept;
void operator delete(void* p, size_t) noexcept;
void operator delete[](void* p) noexcept;
void operator delete[](void* p, size_t) noexcept;

#endif  // MEMORY_H
