#include "page_table_manager.h"

#include <kernel/memory.h>

#include "../../include/log.h"
#include "kernel/addr.h"
#include "page_map_indexer.h"

PageTableManager::PageTableManager(PageTable* PML4Address) {
    this->PML4 = PML4Address;
}

void PageTableManager::map_range(virt_addr_t virt_start, phys_addr_t phys_start, const size_t size, const uint64_t flags) const {
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        map_memory(virt_add(virt_start, offset), phys_add(phys_start, offset), flags);
    }
}

static void invlpg(virt_addr_t addr) {
    asm volatile("invlpg (%0)" : : "r"(virt_raw(addr)) : "memory");
}

void PageTableManager::map_memory(virt_addr_t virt_addr, phys_addr_t phys_addr, uint64_t flags) const {
    PageMapIndexer indexer(virt_addr);

    auto ensure_table = [&](PageTable* parent, uint16_t index) -> PageTable* {
        PageDirectoryEntry& entry = parent->entries[index];

        if (!entry.get_flag(PT_Flag::Present)) {
            phys_addr_t new_phys = kernel::memory::request_page_phys();
            if (phys_null(new_phys)) return nullptr;

            auto* new_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(new_phys)));
            memset(new_virt, 0, 0x1000);

            entry.set_address(new_phys);
        }
        entry.set_flag(PT_Flag::Present,   true);
        entry.set_flag(PT_Flag::ReadWrite,  true);
        entry.set_flag(PT_Flag::UserSuper,  true);

        return static_cast<PageTable*>(virt_ptr(phys_to_virt(entry.get_address())));
    };

    phys_addr_t pml4_phys = make_phys(reinterpret_cast<uint64_t>(PML4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageTable* PDP = ensure_table(pml4_virt, indexer.PDP_i);
    if (!PDP) return;

    PageTable* PD = ensure_table(PDP, indexer.PD_i);
    if (!PD) return;

    PageTable* PT = ensure_table(PD, indexer.PT_i);
    if (!PT) return;

    PageDirectoryEntry& final_entry = PT->entries[indexer.P_i];
    final_entry.set_address(phys_addr);

    flags |= (1ULL << PT_Flag::Present) | (1ULL << PT_Flag::ReadWrite);
    for (int bit = 0; bit < 64; ++bit) {
        if (flags & (1ULL << bit)) final_entry.set_flag(static_cast<PT_Flag>(bit), true);
    }
    PT->entries[indexer.P_i] = final_entry;

    invlpg(virt_addr);
}

static bool is_table_empty(PageTable* table) {
    if (!table) return true;
    for (auto& entry : table->entries) {
        if (entry.get_flag(PT_Flag::Present)) return false;
    }
    return true;
}

void PageTableManager::unmap_range(virt_addr_t virt_start, size_t size) const {
    for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
        unmap_memory(virt_add(virt_start, offset));
    }
}

void PageTableManager::unmap_memory(virt_addr_t virt_addr) const {
    PageMapIndexer indexer(virt_addr);

    phys_addr_t pml4_phys = make_phys(reinterpret_cast<uint64_t>(PML4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry& pml4_entry = pml4_virt->entries[indexer.PDP_i];
    if (!pml4_entry.get_flag(PT_Flag::Present)) return;

    auto* PDP = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_entry.get_address())));
    PageDirectoryEntry& pdp_entry = PDP->entries[indexer.PD_i];
    if (!pdp_entry.get_flag(PT_Flag::Present)) return;

    auto* PD = static_cast<PageTable*>(virt_ptr(phys_to_virt(pdp_entry.get_address())));
    PageDirectoryEntry& pd_entry = PD->entries[indexer.PT_i];
    if (!pd_entry.get_flag(PT_Flag::Present)) return;

    auto* PT = static_cast<PageTable*>(virt_ptr(phys_to_virt(pd_entry.get_address())));
    PageDirectoryEntry& page_entry = PT->entries[indexer.P_i];
    if (!page_entry.get_flag(PT_Flag::Present)) return;

    page_entry.Value = 0;
    invlpg(virt_addr);

    if (is_table_empty(PT)) {
        kernel::memory::free_page_phys(pd_entry.get_address());
        pd_entry.Value = 0;
    } else return;

    if (is_table_empty(PD)) {
        kernel::memory::free_page_phys(pdp_entry.get_address());
        pdp_entry.Value = 0;
    } else return;

    if (is_table_empty(PDP)) {
        kernel::memory::free_page_phys(pml4_entry.get_address());
        pml4_entry.Value = 0;
    }
}

bool PageTableManager::is_mapped(virt_addr_t virt_addr) const {
    PageMapIndexer indexer(virt_addr);

    phys_addr_t pml4_phys = make_phys(reinterpret_cast<uint64_t>(PML4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry pde = pml4_virt->entries[indexer.PDP_i];
    if (!pde.get_flag(PT_Flag::Present)) return false;

    auto* PDP = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PDP->entries[indexer.PD_i];
    if (!pde.get_flag(PT_Flag::Present)) return false;

    auto* PD = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PD->entries[indexer.PT_i];
    if (!pde.get_flag(PT_Flag::Present)) return false;

    auto* PT = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PT->entries[indexer.P_i];
    return pde.get_flag(PT_Flag::Present);
}

phys_addr_t PageTableManager::get_physical_address(virt_addr_t virt_addr) const {
    PageMapIndexer indexer(virt_addr);

    phys_addr_t pml4_phys = make_phys(reinterpret_cast<uint64_t>(PML4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry pde = pml4_virt->entries[indexer.PDP_i];
    if (!pde.get_flag(PT_Flag::Present)) return make_phys(0);

    auto* PDP = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PDP->entries[indexer.PD_i];
    if (!pde.get_flag(PT_Flag::Present)) return make_phys(0);

    auto* PD = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PD->entries[indexer.PT_i];
    if (!pde.get_flag(PT_Flag::Present)) return make_phys(0);

    auto* PT = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = PT->entries[indexer.P_i];
    if (!pde.get_flag(PT_Flag::Present)) return make_phys(0);

    uint64_t offset = virt_raw(virt_addr) & 0xFFF;
    return phys_add(pde.get_address(), offset);
}