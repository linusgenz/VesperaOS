#include "page_table_manager.h"

#include <vespera/mm/addr.h>
#include "page_map_indexer.h"
#include <vespera/mm/memory.h>
#include <klib/string.h>

PageTableManager::PageTableManager(PageTable* pml4_address)
    : pml4(pml4_address) {
    spinlock_.init("page_table_manager");
}

void PageTableManager::map_range(
    const virt_addr_t virt_start, const phys_addr_t phys_start, const usize size, const u64 flags
) const {
    for (usize offset = 0; offset < size; offset += PAGE_SIZE) {
        map_memory(virt_add(virt_start, offset), phys_add(phys_start, offset), flags);
    }
}

static void invlpg(virt_addr_t addr) {
    asm volatile("invlpg (%0)" : : "r"(virt_raw(addr)) : "memory");
}

void PageTableManager::map_memory(virt_addr_t virt_addr, phys_addr_t phys_addr, u64 flags) const {
    SpinlockGuard guard(spinlock_);
    const PageMapIndexer indexer(virt_addr);

    auto ensure_table = [&](PageTable* parent, const u16 index) -> PageTable* {
        PageDirectoryEntry& entry = parent->entries[index];

        if (!entry.get_flag(PtFlag::Present)) {
            const phys_addr_t new_phys = kernel::memory::request_page_phys();
            if (phys_null(new_phys)) return nullptr;

            auto* new_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(new_phys)));
            memset(new_virt, 0, 0x1000);

            entry.set_address(new_phys);
        }
        entry.set_flag(PtFlag::Present, true);
        entry.set_flag(PtFlag::ReadWrite, true);
        entry.set_flag(PtFlag::UserSuper, true);

        return static_cast<PageTable*>(virt_ptr(phys_to_virt(entry.get_address())));
    };

    const phys_addr_t pml4_phys = make_phys(reinterpret_cast<u64>(pml4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageTable* pdp = ensure_table(pml4_virt, indexer.pdp_i);
    if (!pdp) return;

    PageTable* pd = ensure_table(pdp, indexer.pd_i);
    if (!pd) return;

    PageTable* pt = ensure_table(pd, indexer.pt_i);
    if (!pt) return;

    PageDirectoryEntry& final_entry = pt->entries[indexer.p_i];
    final_entry.set_address(phys_addr);

    flags |= (1ULL << PtFlag::Present) | (1ULL << PtFlag::ReadWrite);
    for (int bit = 0; bit < 64; ++bit) {
        if (flags & (1ULL << bit)) final_entry.set_flag(static_cast<PtFlag>(bit), true);
    }
    pt->entries[indexer.p_i] = final_entry;

    invlpg(virt_addr);
}

static bool is_table_empty(PageTable* table) {
    if (!table) return true;
    for (auto& entry : table->entries) {
        if (entry.get_flag(PtFlag::Present)) return false;
    }
    return true;
}

void PageTableManager::unmap_range(const virt_addr_t virt_start, const usize size) const {
    for (usize offset = 0; offset < size; offset += PAGE_SIZE) {
        unmap_memory(virt_add(virt_start, offset));
    }
}

void PageTableManager::unmap_memory(const virt_addr_t virt_addr) const {
    SpinlockGuard guard(spinlock_);
    const PageMapIndexer indexer(virt_addr);

    const phys_addr_t pml4_phys = make_phys(reinterpret_cast<u64>(pml4));
    auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry& pml4_entry = pml4_virt->entries[indexer.pdp_i];
    if (!pml4_entry.get_flag(PtFlag::Present)) return;

    auto* pdp = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_entry.get_address())));
    PageDirectoryEntry& pdp_entry = pdp->entries[indexer.pd_i];
    if (!pdp_entry.get_flag(PtFlag::Present)) return;

    auto* pd = static_cast<PageTable*>(virt_ptr(phys_to_virt(pdp_entry.get_address())));
    PageDirectoryEntry& pd_entry = pd->entries[indexer.pt_i];
    if (!pd_entry.get_flag(PtFlag::Present)) return;

    auto* pt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pd_entry.get_address())));
    PageDirectoryEntry& page_entry = pt->entries[indexer.p_i];
    if (!page_entry.get_flag(PtFlag::Present)) return;

    page_entry.value = 0;
    invlpg(virt_addr);

    if (is_table_empty(pt)) {
        kernel::memory::free_page_phys(pd_entry.get_address());
        pd_entry.value = 0;
    } else
        return;

    if (is_table_empty(pd)) {
        kernel::memory::free_page_phys(pdp_entry.get_address());
        pdp_entry.value = 0;
    } else
        return;

    if (is_table_empty(pdp)) {
        kernel::memory::free_page_phys(pml4_entry.get_address());
        pml4_entry.value = 0;
    }
}

bool PageTableManager::is_mapped(const virt_addr_t virt_addr) const {
    SpinlockGuard guard(spinlock_);
    const PageMapIndexer indexer(virt_addr);

    const phys_addr_t pml4_phys = make_phys(reinterpret_cast<u64>(pml4));
    const auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry pde = pml4_virt->entries[indexer.pdp_i];
    if (!pde.get_flag(PtFlag::Present)) return false;

    const auto* pdp = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pdp->entries[indexer.pd_i];
    if (!pde.get_flag(PtFlag::Present)) return false;

    const auto* pd = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pd->entries[indexer.pt_i];
    if (!pde.get_flag(PtFlag::Present)) return false;

    const auto* pt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pt->entries[indexer.p_i];
    return pde.get_flag(PtFlag::Present);
}

phys_addr_t PageTableManager::get_physical_address(const virt_addr_t virt_addr) const {
    SpinlockGuard guard(spinlock_);
    const PageMapIndexer indexer(virt_addr);

    const phys_addr_t pml4_phys = make_phys(reinterpret_cast<u64>(pml4));
    const auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    PageDirectoryEntry pde = pml4_virt->entries[indexer.pdp_i];
    if (!pde.get_flag(PtFlag::Present)) return make_phys(0);

    const auto* pdp = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pdp->entries[indexer.pd_i];
    if (!pde.get_flag(PtFlag::Present)) return make_phys(0);

    const auto* pd = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pd->entries[indexer.pt_i];
    if (!pde.get_flag(PtFlag::Present)) return make_phys(0);

    const auto* pt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pde.get_address())));
    pde = pt->entries[indexer.p_i];
    if (!pde.get_flag(PtFlag::Present)) return make_phys(0);

    const u64 offset = virt_raw(virt_addr) & 0xFFF;
    return phys_add(pde.get_address(), offset);
}

void PageTableManager::destroy_userspace() const {
    const phys_addr_t pml4_phys = make_phys(reinterpret_cast<u64>(pml4));
    const auto* pml4_virt = static_cast<PageTable*>(virt_ptr(phys_to_virt(pml4_phys)));

    // only lower half (userspace)
    for (usize i = 0; i < 256; ++i) {
        PageDirectoryEntry entry = pml4_virt->entries[i];

        if (!entry.get_flag(PtFlag::Present))
            continue;

        auto* pdp = static_cast<PageTable*>(
            virt_ptr(phys_to_virt(entry.get_address()))
        );

        destroy_level(pdp, 3);

        kernel::memory::free_page_phys(entry.get_address());

        entry.value = 0;
    }

    kernel::memory::free_page_phys(
        make_phys(reinterpret_cast<u64>(pml4))
    );
}

void PageTableManager::destroy_level(PageTable* table, int level) const {
    if (!table) return;

    for (auto & entry : table->entries) {
        if (!entry.get_flag(PtFlag::Present))
            continue;

        const phys_addr_t phys = entry.get_address();

        if (level == 1) {
            // PT level -> actual mapped user page
            kernel::memory::free_page_phys(phys);
        } else {
            auto* next = static_cast<PageTable*>(
                virt_ptr(phys_to_virt(phys))
            );

            destroy_level(next, level - 1);

            kernel::memory::free_page_phys(phys);
        }

        entry.value = 0;
    }
}