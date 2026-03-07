#include <vespera/kernel_utils.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include <vespera/mm/addr.h>
#include "../paging/page_frame_allocator.h"
#include "../paging/page_table_manager.h"
#include "heap.h"

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* m_map, const size_t m_map_entries, const size_t m_map_desc_size) {
    uint64_t memory_size_bytes = 0;  // static
    // if (memory_size_bytes > 0) return memory_size_bytes;

    for (size_t i = 0; i < m_map_entries; i++) {
        const auto* desc =
            reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(reinterpret_cast<uint64_t>(m_map) + (i * m_map_desc_size));
        if (desc->type != 7) continue;
        memory_size_bytes += desc->num_pages * 4096;
    }

    return memory_size_bytes;
}

extern "C" {
    void memset(void* dest, uint8_t val, uint64_t num) {
        for (uint64_t i = 0; i < num; i++) {
            *reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(dest) + i) = val;
        }
    }

    void* memcpy(void* dest, const void* src, size_t len) {
        auto* d = static_cast<char*>(dest);
        auto* s = static_cast<const char*>(src);
        while (len--) *d++ = *s++;
        return dest;
    }

    int memcmp(const void* ptr1, const void* ptr2, const size_t num) {
        const auto* a = static_cast<const uint8_t*>(ptr1);
        const auto* b = static_cast<const uint8_t*>(ptr2);
        for (size_t i = 0; i < num; i++) {
            if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
        }
        return 0;
    }

    void* memmove(void* dest, const void* src, size_t len) {
        auto* d = static_cast<char*>(dest);
        if (auto s = static_cast<const char*>(src); d < s)
            while (len--) *d++ = *s++;
        else {
            auto lasts = const_cast<char*>(s + (len - 1));
            auto* lastd = d + (len - 1);
            while (len--) *lastd-- = *lasts--;
        }
        return dest;
    }
}

virt_addr_t phys_to_virt(phys_addr_t addr) {
    return virt_from_raw(addr.raw + g_hhdm_offset);
}

phys_addr_t virt_to_phys(virt_addr_t addr) {
    return make_phys(virt_raw(addr) - g_hhdm_offset);
}

namespace kernel::memory {
    static PageFrameAllocator page_frame_allocator;
    static PageTableManager page_table_manager = nullptr;

    void initialize_page_frame_allocator(void* efi_memory_map, const size_t map_size, const size_t desc_size) {
        page_frame_allocator.read_efi_memory_map(
            static_cast<EFI_MEMORY_DESCRIPTOR*>(efi_memory_map), map_size, desc_size
        );
    }

    void initialize_page_table_manager(BootInfo* boot_info) {
        g_kernel_phys_base = boot_info->kernel_phys_base;
        g_kernel_virt_base = boot_info->kernel_virt_base;

        auto* pml4 = reinterpret_cast<PageTable*>(phys_raw(request_page_phys()));
        memset(pml4, 0, 0x1000);

        page_table_manager = PageTableManager(pml4);

        uint64_t max_phys = 0;
        const uint64_t entries = boot_info->m_map_size / boot_info->m_map_desc_size;
        for (uint64_t i = 0; i < entries; i++) {
            auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                reinterpret_cast<uint64_t>(boot_info->m_map) + i * boot_info->m_map_desc_size
            );
            if (const uint64_t end = desc->phys_addr + desc->num_pages * 0x1000; end > max_phys) max_phys = end;
        }

        max_phys = (max_phys + 0x1FFFFF) & ~0x1FFFFFULL;

        for (uint64_t phys = 0; phys < max_phys; phys += 0x1000) {
            page_table_manager.map_memory(virt_from_raw(boot_info->hhdm_offset + phys), make_phys(phys), 0);
        }

        const uint64_t k_virt_start = reinterpret_cast<uint64_t>(&kernel_start);
        const uint64_t k_virt_end = reinterpret_cast<uint64_t>(&kernel_end);
        for (uint64_t virt = k_virt_start; virt < k_virt_end; virt += 0x1000) {
            uint64_t phys = virt - g_kernel_virt_base + g_kernel_phys_base;
            page_table_manager.map_memory(virt_from_raw(virt), make_phys(phys), 0);
        }
    }

    // Page Table Manager

    void map_memory(virt_addr_t virt_addr, phys_addr_t phys_addr, const uint64_t flags) {
        page_table_manager.map_memory(virt_addr, phys_addr, flags);
    }

    void map_range(virt_addr_t virt_start, phys_addr_t phys_start, const size_t size, const uint64_t flags) {
        page_table_manager.map_range(virt_start, phys_start, size, flags);
    }

    void unmap_memory(virt_addr_t virt_addr) {
        page_table_manager.unmap_memory(virt_addr);
    }

    void unmap_range(virt_addr_t virt_start, const size_t size) {
        page_table_manager.unmap_range(virt_start, size);
    }

    bool is_mapped(virt_addr_t virt_addr) {
        return page_table_manager.is_mapped(virt_addr);
    }

    phys_addr_t get_physical_address(virt_addr_t virt_addr) {
        return page_table_manager.get_physical_address(virt_addr);
    }

    uintptr_t get_pagetable_address() {
        return reinterpret_cast<uintptr_t>(page_table_manager.pml4);
    }

    // Page Frame Allocator

    void lock_page(phys_addr_t phys_addr) {
        page_frame_allocator.lock_page(reinterpret_cast<void*>(phys_raw(phys_addr)));
    }

    void lock_pages(phys_addr_t phys_addr, uint64_t page_count) {
        page_frame_allocator.lock_pages(reinterpret_cast<void*>(phys_raw(phys_addr)), page_count);
    }

    phys_addr_t request_page_phys() {
        return make_phys(page_frame_allocator.request_page());
    }

    virt_addr_t request_page() {
        uint64_t phys = page_frame_allocator.request_page();
        if (!phys) return make_virt(nullptr);
        return phys_to_virt(make_phys(phys));
    }

    phys_addr_t request_pages_phys(size_t page_count) {
        return make_phys(page_frame_allocator.request_pages(page_count));
    }

    virt_addr_t request_pages(size_t page_count) {
        uint64_t phys = page_frame_allocator.request_pages(page_count);
        if (!phys) return make_virt(nullptr);
        return phys_to_virt(make_phys(phys));
    }

    void free_page(virt_addr_t virt_addr) {
        page_frame_allocator.free_page(phys_raw(virt_to_phys(virt_addr)));
    }

    void free_page_phys(phys_addr_t phys_addr) {
        page_frame_allocator.free_page(phys_raw(phys_addr));
    }

    void free_pages(virt_addr_t virt_addr, uint64_t page_count) {
        page_frame_allocator.free_pages(phys_raw(virt_to_phys(virt_addr)), page_count);
    }

    void free_pages_phys(phys_addr_t phys_addr, uint64_t page_count) {
        page_frame_allocator.free_pages(phys_raw(phys_addr), page_count);
    }

    void relocate_bitmap_to_hhdm() {
        page_frame_allocator.relocate_bitmap_to_hhdm();
    }

    // Memory Statistics
    uint64_t get_total_ram() {
        return page_frame_allocator.get_total_ram();
    }
    uint64_t get_free_ram() {
        return page_frame_allocator.get_free_ram();
    }
    uint64_t get_used_ram() {
        return page_frame_allocator.get_used_ram();
    }
    uint64_t get_reserved_ram() {
        return page_frame_allocator.get_reserved_ram();
    }

    // Heap
    static bool heap_initialized = false;
    static Spinlock heap_lock;

    void initialize_heap(virt_addr_t heap_start, size_t page_count) {
        ::initialize_heap(heap_start, page_count);
        heap_lock.init("kernel_heap_lock");
        heap_initialized = true;
    }

    void* malloc(const size_t size) {
        if (!heap_initialized) return nullptr;
        SpinlockGuard guard(heap_lock);
        return ::kmalloc(size);
    }

    void free(void* addr) {
        if (!heap_initialized) return;
        SpinlockGuard guard(heap_lock);
        ::kfree(addr);
    }

    void* alloc_aligned(const size_t size, const size_t alignment, const size_t boundary) {
        if (!heap_initialized) return nullptr;
        SpinlockGuard guard(heap_lock);
        return ::kalloc_aligned(size, alignment, boundary);
    }

    void free_aligned(void* aligned_ptr) {
        if (!heap_initialized) return;
        SpinlockGuard guard(heap_lock);
        return ::kfree_aligned(aligned_ptr);
    }

    void print_heap_stats() {
        ::print_heap_stats();
    }

    void* realloc(void* old_ptr, const size_t old_size, const size_t new_size) {
        if (!heap_initialized) return nullptr;
        SpinlockGuard guard(heap_lock);
        return ::krealloc(old_ptr, old_size, new_size);
    }
}  // namespace kernel::memory
