#include "page_table_manager.h"
#include "page_map_indexer.h"
#include <kernel/memory.h>

#include "../../include/log.h"

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
            uint64_t new_phys = kernel::memory::request_page_phys();
            if (!new_phys) return nullptr;

            auto* new_virt = static_cast<PageTable*>(phys_to_virt(new_phys));
            memset(new_virt, 0, 0x1000);

            entry.set_address(new_phys >> 12);
        }
        entry.set_flag(PT_Flag::Present, true);
        entry.set_flag(PT_Flag::ReadWrite, true);
        entry.set_flag(PT_Flag::UserSuper, true);

        return static_cast<PageTable*>(phys_to_virt(entry.get_address() << 12));
    };

    auto* pml4_virt = static_cast<PageTable*>(phys_to_virt(reinterpret_cast<uint64_t>(PML4)));

    PageTable *PDP = ensure_table(pml4_virt, indexer.PDP_i);
    if (!PDP) return;

    PageTable *PD = ensure_table(PDP, indexer.PD_i);
    if (!PD) return;

    PageTable *PT = ensure_table(PD, indexer.PT_i);
    if (!PT) return;

    PageDirectoryEntry &final_entry = PT->entries[indexer.P_i];
    final_entry.set_address(reinterpret_cast<uint64_t>(physical_memory) >> 12);

    flags |= (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite);
    for (int bit = 0; bit < 64; ++bit) {
        if (flags & (1ULL << bit))
            final_entry.set_flag(static_cast<PT_Flag>(bit), true);
    }
    PT->entries[indexer.P_i] = final_entry;

    invlpg(virtual_memory);
}

void PageTableManager::set_user_flags(void* virtual_memory, size_t size) const {
    const uintptr_t start = reinterpret_cast<uintptr_t>(virtual_memory) & ~0xFFFULL;
    const uintptr_t end = start + size;

    auto* pml4_virt = static_cast<PageTable*>(phys_to_virt(reinterpret_cast<uint64_t>(PML4)));

    for (uintptr_t addr = start; addr < end; addr += 0x1000) {
        const PageMapIndexer indexer(addr);

        PageDirectoryEntry& pml4_entry = pml4_virt->entries[indexer.PDP_i];
        if (!pml4_entry.get_flag(PT_Flag::Present)) continue;
        auto* PDP = static_cast<PageTable*>(phys_to_virt(pml4_entry.get_address() << 12));

        PageDirectoryEntry& pdp_entry = PDP->entries[indexer.PD_i];
        if (!pdp_entry.get_flag(PT_Flag::Present)) continue;
        auto* PD = static_cast<PageTable*>(phys_to_virt(pdp_entry.get_address() << 12));

        PageDirectoryEntry& pd_entry = PD->entries[indexer.PT_i];
        if (!pd_entry.get_flag(PT_Flag::Present)) continue;
        auto* PT = static_cast<PageTable*>(phys_to_virt(pd_entry.get_address() << 12));

        pml4_entry.set_flag(PT_Flag::UserSuper, true);
        pdp_entry.set_flag(PT_Flag::UserSuper, true);
        pd_entry.set_flag(PT_Flag::UserSuper, true);
        PT->entries[indexer.P_i].set_flag(PT_Flag::UserSuper, true);
    }
}

static bool is_table_empty(PageTable* table) {
    if (!table) return true;
    for (auto & entrie : table->entries) {
        if (entrie.get_flag(PT_Flag::Present)) return false;
    }
    return true;
}

void PageTableManager::unmap_range(void *virt_start,size_t size) const {
    auto vs = reinterpret_cast<uintptr_t>(virt_start);
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        unmap_memory(reinterpret_cast<void*>(vs + offset));
    }
}

void PageTableManager::unmap_memory(void* virtual_memory) const {
    PageMapIndexer indexer(reinterpret_cast<uint64_t>(virtual_memory));

    auto* pml4_virt = static_cast<PageTable*>(phys_to_virt(reinterpret_cast<uint64_t>(PML4)));
    PageDirectoryEntry& pml4_entry = pml4_virt->entries[indexer.PDP_i];
    if (!pml4_entry.get_flag(PT_Flag::Present)) return;

    auto* PDP = static_cast<PageTable*>(phys_to_virt(pml4_entry.get_address() << 12));
    PageDirectoryEntry& pdp_entry = PDP->entries[indexer.PD_i];
    if (!pdp_entry.get_flag(PT_Flag::Present)) return;

    auto* PD = static_cast<PageTable*>(phys_to_virt(pdp_entry.get_address() << 12));
    PageDirectoryEntry& pd_entry = PD->entries[indexer.PT_i];
    if (!pd_entry.get_flag(PT_Flag::Present)) return;

    auto* PT = static_cast<PageTable*>(phys_to_virt(pd_entry.get_address() << 12));
    PageDirectoryEntry& page_entry = PT->entries[indexer.P_i];
    if (!page_entry.get_flag(PT_Flag::Present)) return;

    page_entry.Value = 0;
    asm volatile("invlpg (%0)" : : "r"(virtual_memory) : "memory");

    if (is_table_empty(PT)) {
        kernel::memory::free_page_phys(pd_entry.get_address() << 12);
        pd_entry.Value = 0;
    } else return;

    if (is_table_empty(PD)) {
        kernel::memory::free_page_phys(pdp_entry.get_address() << 12);
        pdp_entry.Value = 0;
    } else return;

    if (is_table_empty(PDP)) {
        kernel::memory::free_page_phys(pml4_entry.get_address() << 12);
        pml4_entry.Value = 0;
    }
}

bool PageTableManager::is_mapped(void* virtual_memory) const {
    auto indexer = PageMapIndexer(reinterpret_cast<uint64_t>(virtual_memory));

    auto* pml4_virt = static_cast<PageTable*>(phys_to_virt(reinterpret_cast<uint64_t>(PML4)));
    PageDirectoryEntry PDE = pml4_virt->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    auto* PDP = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    auto* PD = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return false;

    auto* PT = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PT->entries[indexer.P_i];
    return PDE.get_flag(PT_Flag::Present);
}

void* PageTableManager::get_physical_address(void* virtual_memory) const {
    const auto indexer = PageMapIndexer(reinterpret_cast<uint64_t>(virtual_memory));

    auto* pml4_virt = static_cast<PageTable*>(phys_to_virt(reinterpret_cast<uint64_t>(PML4)));

    PageDirectoryEntry PDE = pml4_virt->entries[indexer.PDP_i];
    if (!PDE.get_flag(PT_Flag::Present)) return nullptr;

    auto* PDP = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PDP->entries[indexer.PD_i];
    if (!PDE.get_flag(PT_Flag::Present)) return nullptr;

    auto* PD = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PD->entries[indexer.PT_i];
    if (!PDE.get_flag(PT_Flag::Present)) return nullptr;

    auto* PT = static_cast<PageTable*>(phys_to_virt(PDE.get_address() << 12));
    PDE = PT->entries[indexer.P_i];
    if (!PDE.get_flag(PT_Flag::Present)) return nullptr;

    uint64_t phys_base = PDE.get_address() << 12;
    uint64_t offset = reinterpret_cast<uint64_t>(virtual_memory) & 0xFFF;
    return reinterpret_cast<void*>(phys_base + offset);
}