#include "page_table_manager.h"
#include "page_map_indexer.h"
#include <memory.h>
#include <cstddef>

#include "page_frame_allocator.h"

PageTableManager::PageTableManager(PageTable *PML4Address) {
    this->PML4 = PML4Address;
}

void PageTableManager::map_range(void *virt_start, void *phys_start, const size_t size, const uint64_t flags) const {
    auto vs = reinterpret_cast<uintptr_t>(virt_start);
    auto ps = reinterpret_cast<uintptr_t>(phys_start);
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        map_memory(reinterpret_cast<void*>(vs + offset), reinterpret_cast<void*>(ps + offset), flags);
    }
}

static void invlpg(void* addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void PageTableManager::map_memory(void *virtual_memory, void *physical_memory, uint64_t flags) const {
    PageMapIndexer indexer(reinterpret_cast<uint64_t>(virtual_memory));

    auto ensure_table = [&](PageTable *parent, uint16_t index) -> PageTable * {
        PageDirectoryEntry &entry = parent->entries[index];

        if (!entry.get_flag(PT_Flag::Present)) {
            auto *new_table = static_cast<PageTable*>(g_allocator->request_page());
            if (!new_table) return nullptr;
            memset(new_table, 0, 0x1000);
            entry.set_address(reinterpret_cast<uint64_t>(new_table) >> 12);
        }
        // Immer die Flags korrekt setzen
        entry.set_flag(PT_Flag::Present, true);
        entry.set_flag(PT_Flag::ReadWrite, true);
        entry.set_flag(PT_Flag::UserSuper, false);

        return reinterpret_cast<PageTable *>(entry.get_address() << 12);
    };

    PageTable *PDP = ensure_table(PML4, indexer.PDP_i);
    if (!PDP) return;

    PageTable *PD = ensure_table(PDP, indexer.PD_i);
    if (!PD) return;

    PageTable *PT = ensure_table(PD, indexer.PT_i);
    if (!PT) return;

    PageDirectoryEntry &final_entry = PT->entries[indexer.P_i];
    final_entry.set_address(reinterpret_cast<uint64_t>(physical_memory) >> 12);

    // Default Flags (immer present + rw)
    flags |= (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite);

    for (int bit = 0; bit < 64; ++bit) {
        if (flags & (1ULL << bit)) {
            final_entry.set_flag(static_cast<PT_Flag>(bit), true);
        }
    }

    PT->entries[indexer.P_i] = final_entry;

    invlpg(virtual_memory);
}
