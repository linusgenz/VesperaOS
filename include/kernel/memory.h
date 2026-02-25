//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H

#include <kernel/kernel_utils.h>

#include "efi_memory.h"

#define PAGE_SIZE 4096
#define KERNEL_BASE 0xFFFFFFFF80000000

struct PageTable;

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);

void memset(void* dest, uint8_t val, uint64_t num);

void* memcpy(void* dest, const void* src, size_t len);

int memcmp(const void* ptr1, const void* ptr2, size_t num);

void* memmove(void* dest, const void* src, size_t len);

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
    void initialize_page_table_manager();

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

    void free_page(void* virtual_addr);

    void free_pages(void* virtual_addr, uint64_t page_count);

    void lock_page(void* virtual_addr);

    void lock_pages(void* virtual_addr, uint64_t page_count);

    [[nodiscard]] void* request_page();

    [[nodiscard]] void* request_pages(size_t pageCount);

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

inline void *operator new(size_t size) { return kernel::memory::malloc(size); }
inline void *operator new[](size_t size) { return kernel::memory::malloc(size); }
inline void *operator new(size_t size, void *ptr) noexcept { return ptr; }
inline void operator delete(void *p) { return kernel::memory::free(p); }
inline void operator delete(void *ptr, size_t size) { kernel::memory::free(ptr); }
inline void operator delete[](void *p) { kernel::memory::free(p); }
inline void operator delete[](void *p, size_t size) { kernel::memory::free(p); }

#endif //MEMORY_H
