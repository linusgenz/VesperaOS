#include <kernel/memory.h>

#include <log.h>

#include "../paging/page_frame_allocator.h"
#include "../paging/page_table_manager.h"
#include "heap.h"

uint64_t get_memory_size(EFI_MEMORY_DESCRIPTOR* mMap, uint64_t mMapEntries, uint64_t mMapDescSize)
{
    uint64_t memory_size_bytes = 0; // static
    // if (memory_size_bytes > 0) return memory_size_bytes;

    for (int i = 0; i < mMapEntries; i++)
    {
        const auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
            reinterpret_cast<uint64_t>(mMap) + (i * mMapDescSize));
        if (desc->type != 7) continue;
        memory_size_bytes += desc->num_pages * 4096;
    }

    return memory_size_bytes;
}

void memset(void* dest, uint8_t val, uint64_t num)
{
    for (uint64_t i = 0; i < num; i++)
    {
        *reinterpret_cast<uint8_t*>(reinterpret_cast<uint64_t>(dest) + i) = val;
    }
}

void* memcpy(void* dest, const void* src, size_t len)
{
    auto* d = static_cast<char*>(dest);
    auto* s = static_cast<const char*>(src);
    while (len--)
        *d++ = *s++;
    return dest;
}

int memcmp(const void* ptr1, const void* ptr2, const size_t num)
{
    const auto* a = static_cast<const uint8_t*>(ptr1);
    const auto* b = static_cast<const uint8_t*>(ptr2);
    for (size_t i = 0; i < num; i++)
    {
        if (a[i] != b[i])
            return (a[i] < b[i]) ? -1 : 1;
    }
    return 0;
}

void* memmove(void* dest, const void* src, size_t len)
{
    auto* d = static_cast<char*>(dest);
    if (auto s = static_cast<const char*>(src); d < s)
        while (len--)
            *d++ = *s++;
    else
    {
        auto lasts = const_cast<char*>(s + (len - 1));
        auto* lastd = d + (len - 1);
        while (len--)
            *lastd-- = *lasts--;
    }
    return dest;
}

void* phys_to_virt(const uint64_t phys_addr)
{
    return reinterpret_cast<void*>(phys_addr + g_hhdm_offset);
}

namespace kernel::memory
{
    static PageFrameAllocator page_frame_allocator;
    static PageTableManager page_table_manager = nullptr;

    void initialize_page_table_manager(BootInfo* boot_info)
    {
        g_kernel_phys_base = boot_info->kernel_phys_base;
        g_kernel_virt_base = boot_info->kernel_virt_base;

        auto* PML4 = static_cast<PageTable*>(request_page());
        memset(PML4, 0, 0x1000);

        page_table_manager = PageTableManager(PML4);

        uint64_t max_phys = 0;
        const uint64_t entries = boot_info->mMapSize / boot_info->mMapDescSize;
        for (uint64_t i = 0; i < entries; i++)
        {
            auto* desc = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                reinterpret_cast<uint64_t>(boot_info->mMap) + i * boot_info->mMapDescSize);
            uint64_t end = desc->phys_addr + desc->num_pages * 0x1000;
            if (end > max_phys) max_phys = end;
        }

        max_phys = (max_phys + 0x1FFFFF) & ~0x1FFFFFULL;

        for (uint64_t phys = 0; phys < max_phys; phys += 0x1000)
        {
            void* virt = reinterpret_cast<void*>(boot_info->hhdm_offset + phys);
            page_table_manager.map_memory(virt, reinterpret_cast<void*>(phys), 0);
        }

        const uint64_t kVirtStart = reinterpret_cast<uint64_t>(&_KernelStart);
        const uint64_t kVirtEnd = reinterpret_cast<uint64_t>(&_KernelEnd);
        for (uint64_t virt = kVirtStart; virt < kVirtEnd; virt += 0x1000)
        {
            uint64_t phys = virt - g_kernel_virt_base + g_kernel_phys_base;
            page_table_manager.map_memory(
                reinterpret_cast<void*>(virt),
                reinterpret_cast<void*>(phys),
                0
            );
        }
    }

    void map_memory(void* virtual_addr, void* physical_addr, const uint64_t flags)
    {
        page_table_manager.map_memory(virtual_addr, physical_addr, flags);
    }

    void set_user_flags(void* virtual_memory, const size_t size)
    {
        page_table_manager.set_user_flags(virtual_memory, size);
    }

    void map_range(void* virt_start, void* phys_start, const size_t size, const uint64_t flags)
    {
        page_table_manager.map_range(virt_start, phys_start, size, flags);
    }

    void unmap_memory(void* virtual_addr)
    {
        page_table_manager.unmap_memory(virtual_addr);
    }

    void unmap_range(void* virt_start, const size_t size)
    {
        page_table_manager.unmap_range(virt_start, size);
    }

    bool is_mapped(void* virtual_addr)
    {
        return page_table_manager.is_mapped(virtual_addr);
    }

    uintptr_t get_pagetable_address()
    {
        return reinterpret_cast<uintptr_t>(page_table_manager.PML4);
    }

    void* get_physical_address(void* virtual_addr)
    {
        return page_table_manager.get_physical_address(virtual_addr);
    }

    // Page Frame Allocator
    void initialize_page_frame_allocator(void* efi_memory_map, const size_t map_size, const size_t desc_size)
    {
        page_frame_allocator.read_efi_memory_map(static_cast<EFI_MEMORY_DESCRIPTOR*>(efi_memory_map), map_size,
                                                 desc_size);
    }

    void lock_page(void* virtual_addr)
    {
        page_frame_allocator.lock_page(virtual_addr);
    }

    void lock_pages(void* virtual_addr, uint64_t page_count)
    {
        page_frame_allocator.lock_pages(virtual_addr, page_count);
    }

    void* request_pages(size_t pageCount)
    {
        const uint64_t phys = page_frame_allocator.request_pages(pageCount);
        if (!phys) return nullptr;
        return phys_to_virt(phys);
    }

    uint64_t request_pages_phys(size_t pageCount)
    {
        return page_frame_allocator.request_pages(pageCount);
    }

    void* request_page()
    {
        const uint64_t phys = page_frame_allocator.request_page();
        if (!phys) return nullptr;
        return phys_to_virt(phys);
    }

    uint64_t request_page_phys()
    {
        return page_frame_allocator.request_page();
    }

    void free_page(void* virtual_addr)
    {
        page_frame_allocator.free_page(virtual_addr);
    }

    void free_pages(void* virtual_addr, const uint64_t page_count)
    {
        page_frame_allocator.free_pages(virtual_addr, page_count);
    }

    void relocate_bitmap_to_hhdm()
    {
        page_frame_allocator.relocate_bitmap_to_hhdm();
    }

    uint64_t get_total_ram()
    {
        return page_frame_allocator.get_total_ram();
    }

    uint64_t get_free_ram()
    {
        return page_frame_allocator.get_free_ram();
    }

    uint64_t get_used_ram()
    {
        return page_frame_allocator.get_used_ram();
    }

    uint64_t get_reserved_ram()
    {
        return page_frame_allocator.get_reserved_ram();
    }

    // Heap
    static bool heap_initialized = false;
    static spinlock_t heap_lock;

    void initialize_heap(void* heap_start, size_t page_count)
    {
        ::initialize_heap(heap_start, page_count);
        heap_lock.init("kernel_heap_lock");
        heap_initialized = true;
    }

    void* malloc(const size_t size)
    {
        if (!heap_initialized) return nullptr;
        spinlock_guard guard(heap_lock);
        return ::malloc(size);
    }

    void free(void* addr)
    {
        if (!heap_initialized) return;
        spinlock_guard guard(heap_lock);
        ::free(addr);
    }

    void* alloc_aligned(const size_t size, const size_t alignment, const size_t boundary)
    {
        if (!heap_initialized) return nullptr;
        spinlock_guard guard(heap_lock);
        return ::alloc_aligned(size, alignment, boundary);
    }


    void free_aligned(void* aligned_ptr)
    {
        if (!heap_initialized) return;
        spinlock_guard guard(heap_lock);
        return ::free_aligned(aligned_ptr);
    }

    void print_heap_stats()
    {
        ::print_heap_stats();
    }

    void* realloc(void* old_ptr, const size_t old_size, const size_t new_size)
    {
        if (!heap_initialized) return nullptr;
        spinlock_guard guard(heap_lock);
        return ::realloc(old_ptr, old_size, new_size);
    }
} // namespace kernel::memory
