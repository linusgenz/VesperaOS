//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H

#include <kernel/kernel_utils.h>

#include "efi_memory.h"

#define PAGE_SIZE 4096
inline uint64_t g_hhdm_offset = 0;
inline uint64_t g_kernel_phys_base = 0;
inline uint64_t g_kernel_virt_base = 0;

struct PageTable;

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);

void memset(void* dest, uint8_t val, uint64_t num);

void* memcpy(void* dest, const void* src, size_t len);

int memcmp(const void* ptr1, const void* ptr2, size_t num);

void* memmove(void* dest, const void* src, size_t len);

void* phys_to_virt(uint64_t phys_addr);

enum PT_Flag
{
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
    NX = 63 // only if supported
};

namespace kernel::memory
{
    void initialize_memory(BootInfo* bootInfo);

    // Page Table Management
    void initialize_page_table_manager(BootInfo* bootInfo);

    void map_memory(void* virtual_addr, void* physical_addr, uint64_t flags = 0);

    void set_user_flags(void* virtual_memory, size_t size);

    void map_range(void* virt_start, void* phys_start, size_t size, uint64_t flags = 0);

    void unmap_range(void* virt_start, size_t size);

    void unmap_memory(void* virtual_addr);

    bool is_mapped(void* virtual_addr);

    uintptr_t get_pagetable_address();

    void* get_physical_address(void* virtual_addr);

    // Page Frame Allocator
    void initialize_page_frame_allocator(void* efi_memory_map, size_t map_size, size_t desc_size);

    void free_page(const void* virtual_addr);

    void free_page_phys(uint64_t phys_addr);

    void free_pages(const void* virtual_addr, uint64_t page_count);

    void free_pages_phys(uint64_t phys_addr, uint64_t page_count);

    void lock_page(void* virtual_addr);

    void lock_pages(void* virtual_addr, uint64_t page_count);

    void relocate_bitmap_to_hhdm();

    [[nodiscard]] void* request_page();

    [[nodiscard]] uint64_t request_page_phys();

    [[nodiscard]] void* request_pages(size_t pageCount);

    [[nodiscard]] uint64_t request_pages_phys(size_t pageCount);

    [[nodiscard]] uint64_t get_free_ram();

    [[nodiscard]] uint64_t get_used_ram();

    [[nodiscard]] uint64_t get_reserved_ram();

    [[nodiscard]] uint64_t get_total_ram();

    // Heap Allocator
    void initialize_heap(void* heap_start, size_t page_count);

    void* alloc_aligned(size_t size, size_t alignment, size_t boundary = 0);

    void free_aligned(void* aligned_ptr);

    [[nodiscard]] void* malloc(size_t size);

    void free(void* addr);

    void* realloc(void* old_ptr, size_t old_size, size_t new_size);

    void print_heap_stats();
} // namespace kernel::memory

void* operator new(size_t size);
void* operator new[](size_t size);
inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
void operator delete(void* p) noexcept;
void operator delete(void* p, size_t) noexcept;
void operator delete[](void* p) noexcept;
void operator delete[](void* p, size_t) noexcept;

#endif //MEMORY_H
