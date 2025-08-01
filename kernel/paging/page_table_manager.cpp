#include "../memory/page_table_manager.h"
#include "../memory/page_map_indexer.h"
#include "../include/memory.h"
#include "stdint.h"
#include "../../include/log.h"

PageTableManager::PageTableManager(PageTable *PML4Address) {
    this->PML4 = PML4Address;
}

void PageTableManager::map_range(void *virt_start, void *phys_start, size_t size, uint64_t flags) {
    for (size_t offset = 0; offset < size; offset += 0x1000) {
        map_memory(virt_start + offset, phys_start + offset, flags);
    }
}

void PageTableManager::map_memory(void* virtual_memory, void* physical_memory, uint64_t flags) {
    PageMapIndexer indexer((uint64_t)virtual_memory);

    auto ensure_table = [&](PageTable* parent, uint16_t index, uint64_t flags) -> PageTable* {
        PageDirectoryEntry& entry = parent->entries[index];

        if (!entry.get_flag(PT_Flag::Present)) {
            PageTable* new_table = (PageTable*)kernel::memory::request_page();
            if (!new_table) return nullptr;

            memset(new_table, 0, 0x1000);
            entry.set_address((uint64_t)new_table >> 12);
            entry.set_flag(PT_Flag::Present, true);
            entry.set_flag(PT_Flag::ReadWrite, true);
            entry.set_flag(PT_Flag::UserSuper, true);
        } else if (flags & (1ULL << PT_Flag::UserSuper)) {
            entry.set_flag(PT_Flag::UserSuper, true); // Sicherheitsbedingung
        }

        return (PageTable*)((uint64_t)entry.get_address() << 12);
    };

    PageTable* PDP = ensure_table(PML4, indexer.PDP_i, flags);
    if (!PDP) return;

    PageTable* PD = ensure_table(PDP, indexer.PD_i, flags);
    if (!PD) return;

    PageTable* PT = ensure_table(PD, indexer.PT_i, flags);
    if (!PT) return;

    PageDirectoryEntry& final_entry = PT->entries[indexer.P_i];
    final_entry.set_address((uint64_t)physical_memory >> 12);

    // default flags
    flags |= (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite) | (1ULL << PT_Flag::UserSuper); // TODO temp

    for (int bit = 0; bit < 64; ++bit) {
        if (flags & (1ULL << bit)) {
            final_entry.set_flag(static_cast<PT_Flag>(bit), true);
        }
    }

    PT->entries[indexer.P_i] = final_entry;
}


void PageTableManager::unmap_memory(void *virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t) virtual_memory);

    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }

    PageTable *PDP = (PageTable *) ((uint64_t) PDE.get_address() << 12);

    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }

    PageTable *PD = (PageTable *) ((uint64_t) PDE.get_address() << 12);

    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }

    PageTable *PT = (PageTable *) ((uint64_t) PDE.get_address() << 12);

    PageDirectoryEntry *page_entry = &PT->entries[indexer.P_i];
    if (page_entry->get_flag(PT_Flag::Present)) {
        page_entry->Value = 0;

        asm volatile("invlpg (%0)" : : "r" (virtual_memory) : "memory");
    }

    // TODO: Optional - Prüfe ob ganze Page Tables leer sind und gebe frei
}

bool PageTableManager::is_mapped(void *virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t) virtual_memory);

    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    PageTable *PDP = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    PageTable *PD = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    PageTable *PT = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PT->entries[indexer.P_i];
    return PDE.get_flag(PT_Flag::Present);
}

uint64_t PageTableManager::get_physical_address(void *virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t) virtual_memory);

    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;

    PageTable *PDP = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;

    PageTable *PD = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;

    PageTable *PT = (PageTable *) ((uint64_t) PDE.get_address() << 12);
    PDE = PT->entries[indexer.P_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;

    uint64_t phys_base = PDE.get_address() << 12;
    uint64_t offset = (uint64_t) virtual_memory & 0xFFF;
    return (phys_base + offset);
}
