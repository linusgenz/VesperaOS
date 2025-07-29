#include "../include/page_table_manager.h"
#include "../include/page_map_indexer.h"
#include "../include/page_frame_allocator.h"
#include "../include/memory.h"
#include "stdint.h"

PageTableManager global_page_table_manager = nullptr;

PageTableManager::PageTableManager(PageTable* PML4Address) {
    this->PML4 = PML4Address;
}

void PageTableManager::map_range(uintptr_t virt_start, uintptr_t phys_start, size_t size, uint64_t flags) {
    for (size_t offset = 0; offset < size; offset += 0x1000) {
        map_memory((void*)(virt_start + offset), (void*)(phys_start + offset), flags);
    }
}

void PageTableManager::map_memory(void* virtual_memory, void* physical_memory, uint64_t flags) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtual_memory);
    PageDirectoryEntry PDE;

    PDE = PML4->entries[indexer.PDP_i];

    PageTable* PDP;
    if (!PDE.get_flag(PT_Flag::Present)) {
        PDP = (PageTable*)global_allocator.request_page();
        if (PDP == nullptr) {
            // nicht genug phy memory
            return;
        }
        memset(PDP, 0, 0x1000);
        PDE.set_address((uint64_t)PDP >> 12);
        PDE.set_flag(PT_Flag::Present, true);
        PDE.set_flag(PT_Flag::ReadWrite, true);
        PML4->entries[indexer.PDP_i] = PDE;
    } else {
        PDP = (PageTable*)((uint64_t)PDE.get_address() << 12);
    }
    
    PDE = PDP->entries[indexer.PD_i];
    PageTable* PD;
    if (!PDE.get_flag(PT_Flag::Present)) {
        PD = (PageTable*)global_allocator.request_page();
        memset(PD, 0, 0x1000);
        PDE.set_address((uint64_t)PD >> 12);
        PDE.set_flag(PT_Flag::Present, true);
        PDE.set_flag(PT_Flag::ReadWrite, true);
        PDP->entries[indexer.PD_i] = PDE;
    } else {
        PD = (PageTable*)((uint64_t)PDE.get_address() << 12);
    }

    PDE = PD->entries[indexer.PT_i];
    PageTable* PT;
    if (!PDE.get_flag(PT_Flag::Present)) {
        PT = (PageTable*)global_allocator.request_page();
        memset(PT, 0, 0x1000);
        PDE.set_address((uint64_t)PT >> 12);
        PDE.set_flag(PT_Flag::Present, true);
        PDE.set_flag(PT_Flag::ReadWrite, true);
        PD->entries[indexer.PT_i] = PDE;
    } else {
        PT = (PageTable*)((uint64_t)PDE.get_address() << 12);
    }

    PDE = PT->entries[indexer.P_i];
    PDE.set_address((uint64_t)physical_memory >> 12);
    PDE.set_flag(PT_Flag::Present, true);
    PDE.set_flag(PT_Flag::ReadWrite, true);

    for (int i = 0; i < 64; i++) {
        if ((flags >> i) & 1) {
            PDE.set_flag((PT_Flag)i, true);
        }
    }

    PT->entries[indexer.P_i] = PDE;
}

void PageTableManager::unmap_memory(void* virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtual_memory);

    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }
    
    PageTable* PDP = (PageTable*)((uint64_t)PDE.get_address() << 12);
    
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }
    
    PageTable* PD = (PageTable*)((uint64_t)PDE.get_address() << 12);
    
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) {
        return;
    }
    
    PageTable* PT = (PageTable*)((uint64_t)PDE.get_address() << 12);
    
    PageDirectoryEntry* page_entry = &PT->entries[indexer.P_i];
    if (page_entry->get_flag(PT_Flag::Present)) {
        page_entry->Value = 0;
        
        asm volatile("invlpg (%0)" : : "r" (virtual_memory) : "memory");
    }
    
    // TODO: Optional - Prüfe ob ganze Page Tables leer sind und gebe frei
}

bool PageTableManager::is_mapped(void* virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtual_memory);
    
    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;
    
    PageTable* PDP = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;
    
    PageTable* PD = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;
    
    PageTable* PT = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PT->entries[indexer.P_i];
    return PDE.get_flag(PT_Flag::Present);
}

uint64_t PageTableManager::get_physical_address(void* virtual_memory) {
    PageMapIndexer indexer = PageMapIndexer((uint64_t)virtual_memory);
    
    PageDirectoryEntry PDE = PML4->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;
    
    PageTable* PDP = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;
    
    PageTable* PD = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;
    
    PageTable* PT = (PageTable*)((uint64_t)PDE.get_address() << 12);
    PDE = PT->entries[indexer.P_i];
    if (!PDE.get_flag(PT_Flag::Present)) return 0;
    
    uint64_t phys_base = PDE.get_address() << 12;
    uint64_t offset = (uint64_t)virtual_memory & 0xFFF;
    return (phys_base + offset);
}
