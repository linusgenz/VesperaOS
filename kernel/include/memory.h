//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H
#include <cstdint>
#include <cstddef>
#include "efi_memory.h"

#define PAGE_SIZE 4096

struct PageTable;
struct kprocess_t;

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize);
void memset(void* dest, uint8_t val, uint64_t num);
void *memcpy (void *dest, const void *src, size_t len);
int memcmp(const void* ptr1, const void* ptr2, size_t num);

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
    NX = 63 // only if supported
};

namespace kernel::memory {

    // Page Table Management
    void initialize_page_table_manager();
    void map_memory(void* virtual_addr, void* physical_addr, uint64_t flags = 0, kprocess_t* proc = nullptr);
    void map_range(void* virt_start, void* phys_start, size_t size, uint64_t flags = 0, kprocess_t* proc = nullptr);
    void unmap_memory(void* virtual_addr);
    bool is_mapped(void* virtual_addr);
    uintptr_t get_pagetable_address();
    uint64_t get_physical_address(void* virtual_addr);
    PageTable* create_user_pagetable();

    // Page Frame Allocator
    void initialize_page_frame_allocator(void* efi_memory_map, size_t map_size, size_t desc_size);
    void free_page(void* address);
    void free_pages(void* address, uint64_t page_count);
    void free_user_pages(kprocess_t* proc);
    void lock_page(void* address);
    void lock_pages(void* address, uint64_t page_count);
    void* request_page();
    void *request_pages(size_t pageCount);

    uint64_t get_total_ram();
    uint64_t get_free_ram();
    uint64_t get_used_ram();
    uint64_t get_reserved_ram();

    // Heap Allocator
    void initialize_heap(void* heap_start, size_t page_count);
    void* alloc_aligned(size_t alignment, size_t size, size_t boundary = 0);
    void free_aligned(void* aligned_ptr);
    void* malloc(size_t size);
    void free(void* addr);
    void* realloc(void* oldPtr, size_t oldSize, size_t newSize);

} // namespace kernel::memory

#endif //MEMORY_H
