#include "../memory/page_table_manager.h"
#include "../memory/page_map_indexer.h"
#include "../include/memory.h"
#include <cstdint>
#include "../../include/log.h"

PageTableManager::PageTableManager(PageTable *PML4Address) {
    this->PML4 = PML4Address;
}

void PageTableManager::map_range(void *virt_start, void *phys_start, size_t size, uint64_t flags, kprocess_t* proc) {
    auto vs = reinterpret_cast<uintptr_t>(virt_start);
    auto ps = reinterpret_cast<uintptr_t>(phys_start);
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        map_memory(reinterpret_cast<void*>(vs + offset), reinterpret_cast<void*>(ps + offset), flags, proc);
    }
}

static inline void invlpg(void* addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void PageTableManager::map_memory(void *virtual_memory, void *physical_memory, uint64_t flags, kprocess_t* proc) {
    PageMapIndexer indexer((uint64_t) virtual_memory);

    auto ensure_table = [&](PageTable *parent, uint16_t index) -> PageTable * {
        PageDirectoryEntry &entry = parent->entries[index];

        if (!entry.get_flag(PT_Flag::Present)) {
            PageTable *new_table = (PageTable *) kernel::memory::request_page();
            if (!new_table) return nullptr;
            memset(new_table, 0, 0x1000);
            entry.set_address((uint64_t) new_table >> 12);
        }
        // Immer die Flags korrekt setzen
        entry.set_flag(PT_Flag::Present, true);
        entry.set_flag(PT_Flag::ReadWrite, true);
        entry.set_flag(PT_Flag::UserSuper, true);

        return (PageTable *) ((uint64_t) entry.get_address() << 12);
    };

    PageTable *PDP = ensure_table(PML4, indexer.PDP_i);
    if (!PDP) return;

    PageTable *PD = ensure_table(PDP, indexer.PD_i);
    if (!PD) return;

    PageTable *PT = ensure_table(PD, indexer.PT_i);
    if (!PT) return;

    PageDirectoryEntry &final_entry = PT->entries[indexer.P_i];
    final_entry.set_address((uint64_t) physical_memory >> 12);

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

void PageTableManager::set_user_flags(void* virtual_memory, size_t size) const {
    uintptr_t start = (uintptr_t)virtual_memory & ~0xFFFULL;
    uintptr_t end = start + size;

    for (uintptr_t addr = start; addr < end; addr += 0x1000) {
        PageMapIndexer indexer(addr);

        PageTable *PDP = (PageTable *)((uint64_t)PML4->entries[indexer.PDP_i].get_address() << 12);
        if (!PDP) continue;

        PageTable *PD = (PageTable *)((uint64_t)PDP->entries[indexer.PD_i].get_address() << 12);
        if (!PD) continue;

        PageTable *PT = (PageTable *)((uint64_t)PD->entries[indexer.PT_i].get_address() << 12);
        if (!PT) continue;

        PML4->entries[indexer.PDP_i].set_flag(PT_Flag::UserSuper, true);
        PDP->entries[indexer.PD_i].set_flag(PT_Flag::UserSuper, true);
        PD->entries[indexer.PT_i].set_flag(PT_Flag::UserSuper, true);
        PT->entries[indexer.P_i].set_flag(PT_Flag::UserSuper, true);
    }
}

static bool is_table_empty(PageTable* table) {
    if (!table) return true;
    for (int i = 0; i < 512; ++i) {
        if (table->entries[i].get_flag(PT_Flag::Present)) return false;
    }
    return true;
}

void PageTableManager::unmap_memory(void *virtual_memory) {
    PageMapIndexer indexer((uint64_t) virtual_memory);

    // --- Schritt 1: Holen der Tabellen-Referenzen (Referenzen, keine Kopien) ---
    PageDirectoryEntry &pml4_entry = PML4->entries[indexer.PDP_i];
    if (!pml4_entry.get_flag(PT_Flag::Present)) {
        return;
    }
    PageTable *PDP = reinterpret_cast<PageTable *>((uint64_t)pml4_entry.get_address() << 12);

    PageDirectoryEntry &pdp_entry = PDP->entries[indexer.PD_i];
    if (!pdp_entry.get_flag(PT_Flag::Present)) {
        return;
    }
    PageTable *PD = reinterpret_cast<PageTable *>((uint64_t)pdp_entry.get_address() << 12);

    PageDirectoryEntry &pd_entry = PD->entries[indexer.PT_i];
    if (!pd_entry.get_flag(PT_Flag::Present)) {
        return;
    }
    PageTable *PT = reinterpret_cast<PageTable *>((uint64_t)pd_entry.get_address() << 12);

    // --- Schritt 2: Entferne den PTE (Page Table Entry) ---
    PageDirectoryEntry &page_entry = PT->entries[indexer.P_i];
    if (page_entry.get_flag(PT_Flag::Present)) {
        // lösche Eintrag
        page_entry.Value = 0;

        // invaldiere TLB-Eintrag
        asm volatile("invlpg (%0)" : : "r" (virtual_memory) : "memory");
    } else {
        // nichts zu tun
        return;
    }

    if (is_table_empty(PT)) {
        kernel::memory::free_page(PT);

        pd_entry.Value = 0;
    } else {
        return;
    }


    if (is_table_empty(PD)) {
        kernel::memory::free_page(PD);

        pdp_entry.Value = 0;
    } else {
        return;
    }

    if (is_table_empty(PDP)) {
        kernel::memory::free_page(PDP);

        pml4_entry.Value = 0;
    }
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

PageTable *PageTableManager::create_user_pagetable() {
    auto *user_pml4 = (PageTable *) kernel::memory::request_page();

    kernel::memory::map_memory(user_pml4, user_pml4);
    memset(user_pml4, 0, 0x1000);

    user_pml4->entries[0] = PML4->entries[0];

    return user_pml4;
}