//
// Created by linus on 19.09.24.
//

#ifndef MEMORY_H
#define MEMORY_H


#include <vespera/boot/boot.h>

#include "addr.h"
#include "efi_memory.h"

constexpr usize PAGE_SIZE = 0x1000;
inline u64 g_hhdm_offset = 0;
inline u64 g_kernel_phys_base = 0;
inline u64 g_kernel_virt_base = 0;

struct PageTable;

u64 get_memory_size(EFI_MEMORY_DESCRIPTOR* m_map, usize m_map_entries, usize m_map_desc_size);

extern "C" {
void memset(void* dest, u32 val, u64 num);

void* memcpy(void* dest, const void* src, usize len);

int memcmp(const void* ptr1, const void* ptr2, usize num);

void* memmove(void* dest, const void* src, usize len);
}

inline void memset(const virt_addr_t dest, const u8 val, const u64 num) {
    memset(virt_ptr(dest), val, num);
}

virt_addr_t phys_to_virt(phys_addr_t addr);
phys_addr_t virt_to_phys(virt_addr_t addr);

enum PtFlag {
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
    void initialize_memory(BootInfo* boot_info);

    // Page Table Management
    void initialize_page_table_manager(BootInfo* boot_info);

    void map_memory(virt_addr_t virtual_addr, phys_addr_t physical_addr, u64 flags = 0);

    // void set_user_flags(void* virtual_memory, usize size);

    void map_range(virt_addr_t virt_start, phys_addr_t phys_start, usize size, u64 flags = 0);

    void unmap_range(virt_addr_t virt_start, usize size);

    void unmap_memory(virt_addr_t virtual_addr);

    bool is_mapped(virt_addr_t virtual_addr);

    uptr get_pagetable_address();

    phys_addr_t get_physical_address(virt_addr_t virtual_addr);

    // Page Frame Allocator
    void initialize_page_frame_allocator(void* efi_memory_map, usize map_size, usize desc_size);

    void free_page(virt_addr_t virtual_addr);

    void free_page_phys(phys_addr_t phys_addr);

    void free_pages(virt_addr_t virtual_addr, u64 page_count);

    void free_pages_phys(phys_addr_t phys_addr, u64 page_count);

    void lock_page(phys_addr_t phys_addr);

    void lock_pages(phys_addr_t phys_addr, u64 page_count);

    void relocate_bitmap_to_hhdm();

    [[nodiscard]] virt_addr_t request_page();

    [[nodiscard]] phys_addr_t request_page_phys();

    [[nodiscard]] virt_addr_t request_pages(usize page_count);

    [[nodiscard]] phys_addr_t request_pages_phys(usize page_count);

    [[nodiscard]] u64 get_free_ram();

    [[nodiscard]] u64 get_used_ram();

    [[nodiscard]] u64 get_reserved_ram();

    [[nodiscard]] u64 get_total_ram();

    // Heap Allocator
    void initialize_heap(virt_addr_t heap_start, usize page_count);

    void* alloc_aligned(usize size, usize alignment, usize boundary = 0);

    void free_aligned(void* aligned_ptr);

    [[nodiscard]] void* malloc(usize size);

    void free(void* addr);

    void* realloc(void* old_ptr, usize old_size, usize new_size);

    void print_heap_stats();
}  // namespace kernel::memory

void* operator new(usize size);
void* operator new[](usize size);
inline void* operator new(usize, void* ptr) noexcept {
    return ptr;
}
void operator delete(void* p) noexcept;
void operator delete(void* p, usize) noexcept;
void operator delete[](void* p) noexcept;
void operator delete[](void* p, usize) noexcept;

#endif  // MEMORY_H
