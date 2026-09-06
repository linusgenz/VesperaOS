// intel_ppgtt.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 28.08.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

#include "intel_ppgtt.h"

#include <klib/string.h>
#include <vespera/log.h>
#include <vespera/mm/addr.h>

namespace gpu::intel::core {

namespace {
    [[nodiscard]] gen_pte_t encode_leaf(u64 phys_bytes, PpgttCaching caching, bool writable) {
        gen_pte_t pte = phys_bytes | PPGTT_PAGE_PRESENT;

        if (writable) {
            pte |= PPGTT_PAGE_RW;
        }

        const u32 pat_index = static_cast<u32>(caching);
        if (pat_index & (1u << 0)) pte |= PPGTT_PAGE_PWT;
        if (pat_index & (1u << 1)) pte |= PPGTT_PAGE_PCD;
        if (pat_index & (1u << 2)) pte |= PPGTT_PAGE_PAT;

        return pte;
    }

    [[nodiscard]] gen_pte_t encode_table_pointer(u64 phys_bytes) {
        return phys_bytes | PPGTT_PAGE_RW | PPGTT_PAGE_PRESENT;
    }
}  // namespace

IntelPpgtt::PpgttIndexer::PpgttIndexer(const gfx_addr_t addr) {
    u64 a = gfx_raw(addr);

    pt_i = static_cast<u32>((a / 4096) % PPGTT_ENTRIES_PER_TABLE);
    a /= 4096 * PPGTT_ENTRIES_PER_TABLE;

    pd_i = static_cast<u32>(a % PPGTT_ENTRIES_PER_TABLE);
    a /= PPGTT_ENTRIES_PER_TABLE;

    pdpt_i = static_cast<u32>(a % PPGTT_ENTRIES_PER_TABLE);
    a /= PPGTT_ENTRIES_PER_TABLE;

    pml4_i = static_cast<u32>(a % PPGTT_ENTRIES_PER_TABLE);
}

gen_pte_t* IntelPpgtt::table_virt(const u64 ggtt_addr) const {
    return static_cast<gen_pte_t*>(virt_ptr(ggtt_.gfx_to_virt(make_gfx(ggtt_addr))));
}

void IntelPpgtt::record_table_page(const u64 phys_addr, const u64 ggtt_addr) {
    if (table_page_count_ >= MAX_TABLE_PAGES) {
        Log::error("intel-ppgtt: table_pages_ full, cannot record phys=0x%llx ggtt=0x%llx",
                  phys_addr, ggtt_addr);
        return;
    }
    table_pages_[table_page_count_++] = TablePage{phys_addr, ggtt_addr};
}

u64 IntelPpgtt::ggtt_for_phys(const u64 phys_addr) const {
    for (usize i = 0; i < table_page_count_; i++) {
        if (table_pages_[i].phys_addr == phys_addr) {
            return table_pages_[i].ggtt_addr;
        }
    }
    Log::error("intel-ppgtt: no GGTT mapping recorded for phys=0x%llx", phys_addr);
    return 0;
}

bool IntelPpgtt::alloc_scratch_chain() {
    // Scratch page: the leaf every unmapped PT entry points at. Present +
    // read-only, matching Fuchsia's "readable because overfetch isn't
    // supposed to fault" rationale - our command streams don't rely on
    // overfetch, but read-only-present is still safer bring-up behavior than
    // leaving entries absent (absent -> page fault -> FAULT_AND_HANG).
    auto scratch_page = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    if (virt_null(scratch_page.cpu_addr)) {
        Log::error("intel-ppgtt: scratch page allocation failed");
        return false;
    }
    memset(virt_ptr(scratch_page.cpu_addr), 0, PAGE_SIZE);
    scratch_page_phys_addr_ = phys_raw(scratch_page.phys_addr);
    // The scratch page is only ever used as a PTE leaf target, never as a
    // parent whose entries we need to CPU-edit later, so it doesn't need a
    // table_pages_ entry.

    // Scratch PT: every entry points at the scratch page.
    auto scratch_pt = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    if (virt_null(scratch_pt.cpu_addr)) {
        Log::error("intel-ppgtt: scratch PT allocation failed");
        return false;
    }
    scratch_pt_phys_addr_ = phys_raw(scratch_pt.phys_addr);
    record_table_page(scratch_pt_phys_addr_, gfx_raw(scratch_pt.gfx_addr));
    {
        auto* entries = static_cast<gen_pte_t*>(virt_ptr(scratch_pt.cpu_addr));
        const gen_pte_t leaf = encode_leaf(scratch_page_phys_addr_, PpgttCaching::NONE, false);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = leaf;
        }
    }

    // Scratch PD: every entry points at the scratch PT.
    auto scratch_pd = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    if (virt_null(scratch_pd.cpu_addr)) {
        Log::error("intel-ppgtt: scratch PD allocation failed");
        return false;
    }
    scratch_pd_phys_addr_ = phys_raw(scratch_pd.phys_addr);
    record_table_page(scratch_pd_phys_addr_, gfx_raw(scratch_pd.gfx_addr));
    {
        auto* entries = static_cast<gen_pte_t*>(virt_ptr(scratch_pd.cpu_addr));
        const gen_pte_t ptr = encode_table_pointer(scratch_pt_phys_addr_);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = ptr;
        }
    }

    // Scratch PDPT: every entry points at the scratch PD.
    auto scratch_pdpt = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    if (virt_null(scratch_pdpt.cpu_addr)) {
        Log::error("intel-ppgtt: scratch PDPT allocation failed");
        return false;
    }
    scratch_pdpt_phys_addr_ = phys_raw(scratch_pdpt.phys_addr);
    record_table_page(scratch_pdpt_phys_addr_, gfx_raw(scratch_pdpt.gfx_addr));
    {
        auto* entries = static_cast<gen_pte_t*>(virt_ptr(scratch_pdpt.cpu_addr));
        const gen_pte_t ptr = encode_table_pointer(scratch_pd_phys_addr_);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = ptr;
        }
    }

    return true;
}

bool IntelPpgtt::alloc_root() {
    auto pml4 = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
    if (virt_null(pml4.cpu_addr)) {
        Log::error("intel-ppgtt: PML4 allocation failed");
        return false;
    }
    pml4_phys_addr_ = phys_raw(pml4.phys_addr);
    pml4_virt_ = static_cast<gen_pte_t*>(virt_ptr(pml4.cpu_addr));
    // PML4 is the root - nothing ever reads a "parent" entry pointing at it,
    // so no table_pages_ entry is needed for it either.

    const gen_pte_t ptr = encode_table_pointer(scratch_pdpt_phys_addr_);
    for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
        pml4_virt_[i] = ptr;
    }

    return true;
}

bool IntelPpgtt::init() {
    if (!alloc_scratch_chain()) {
        return false;
    }
    if (!alloc_root()) {
        return false;
    }

    Log::info(
        "intel-ppgtt: initialized, PML4 phys=0x%llx (scratch PDPT=0x%llx PD=0x%llx PT=0x%llx page=0x%llx)",
        pml4_phys_addr_, scratch_pdpt_phys_addr_, scratch_pd_phys_addr_, scratch_pt_phys_addr_,
        scratch_page_phys_addr_
    );

    return true;
}

gen_pte_t* IntelPpgtt::ensure_pt(const u32 pml4_i, const u32 pdpt_i, const u32 pd_i) {
    // PML4 entry: allocate a real PDPT if this slot still points at scratch.
    // pml4_virt_[pml4_i] holds a phys address (that's what the GPU walker
    // needs); ggtt_for_phys() recovers the GGTT mapping we need for CPU
    // access into that child table.
    gen_pte_t pml4_entry = pml4_virt_[pml4_i];
    u64 pdpt_phys_addr = pml4_entry & ~static_cast<u64>(0xFFF);

    if (pdpt_phys_addr == scratch_pdpt_phys_addr_) {
        auto alloc = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-ppgtt: PDPT allocation failed (pml4_i=%u)", pml4_i);
            return nullptr;
        }

        pdpt_phys_addr = phys_raw(alloc.phys_addr);
        const u64 pdpt_ggtt_addr = gfx_raw(alloc.gfx_addr);
        record_table_page(pdpt_phys_addr, pdpt_ggtt_addr);

        auto* entries = static_cast<gen_pte_t*>(virt_ptr(alloc.cpu_addr));
        const gen_pte_t ptr = encode_table_pointer(scratch_pd_phys_addr_);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = ptr;
        }

        pml4_virt_[pml4_i] = encode_table_pointer(pdpt_phys_addr);
    }

    const u64 pdpt_ggtt_addr = ggtt_for_phys(pdpt_phys_addr);
    if (pdpt_ggtt_addr == 0) {
        Log::error("intel-ppgtt: PDPT phys=0x%llx has no GGTT mapping (pml4_i=%u)", pdpt_phys_addr, pml4_i);
        return nullptr;
    }

    // PDPT entry: allocate a real PD if this slot still points at scratch.
    auto* pdpt_virt = table_virt(pdpt_ggtt_addr);
    gen_pte_t pdpt_entry = pdpt_virt[pdpt_i];
    u64 pd_phys_addr = pdpt_entry & ~static_cast<u64>(0xFFF);

    if (pd_phys_addr == scratch_pd_phys_addr_) {
        auto alloc = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-ppgtt: PD allocation failed (pml4_i=%u pdpt_i=%u)", pml4_i, pdpt_i);
            return nullptr;
        }

        pd_phys_addr = phys_raw(alloc.phys_addr);
        const u64 pd_ggtt_addr = gfx_raw(alloc.gfx_addr);
        record_table_page(pd_phys_addr, pd_ggtt_addr);

        auto* entries = static_cast<gen_pte_t*>(virt_ptr(alloc.cpu_addr));
        const gen_pte_t ptr = encode_table_pointer(scratch_pt_phys_addr_);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = ptr;
        }

        pdpt_virt[pdpt_i] = encode_table_pointer(pd_phys_addr);
    }

    const u64 pd_ggtt_addr = ggtt_for_phys(pd_phys_addr);
    if (pd_ggtt_addr == 0) {
        Log::error("intel-ppgtt: PD phys=0x%llx has no GGTT mapping (pml4_i=%u pdpt_i=%u)", pd_phys_addr, pml4_i,
                  pdpt_i);
        return nullptr;
    }

    // PD entry: allocate a real PT if this slot still points at scratch.
    auto* pd_virt = table_virt(pd_ggtt_addr);
    gen_pte_t pd_entry = pd_virt[pd_i];
    u64 pt_phys_addr = pd_entry & ~static_cast<u64>(0xFFF);

    if (pt_phys_addr == scratch_pt_phys_addr_) {
        auto alloc = ggtt_.alloc_persistent(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(alloc.cpu_addr)) {
            Log::error("intel-ppgtt: PT allocation failed (pml4_i=%u pdpt_i=%u pd_i=%u)", pml4_i, pdpt_i, pd_i);
            return nullptr;
        }

        pt_phys_addr = phys_raw(alloc.phys_addr);
        const u64 pt_ggtt_addr = gfx_raw(alloc.gfx_addr);
        record_table_page(pt_phys_addr, pt_ggtt_addr);

        auto* entries = static_cast<gen_pte_t*>(virt_ptr(alloc.cpu_addr));
        const gen_pte_t leaf = encode_leaf(scratch_page_phys_addr_, PpgttCaching::NONE, false);
        for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
            entries[i] = leaf;
        }

        pd_virt[pd_i] = encode_table_pointer(pt_phys_addr);
    }

    const u64 pt_ggtt_addr = ggtt_for_phys(pt_phys_addr);
    if (pt_ggtt_addr == 0) {
        Log::error("intel-ppgtt: PT phys=0x%llx has no GGTT mapping (pml4_i=%u pdpt_i=%u pd_i=%u)", pt_phys_addr,
                  pml4_i, pdpt_i, pd_i);
        return nullptr;
    }

    return table_virt(pt_ggtt_addr);
}

bool IntelPpgtt::insert_range(const gfx_addr_t gpu_addr, const phys_addr_t phys_start, const usize size,
                              const PpgttCaching caching, const bool writable) {
    if (gfx_raw(gpu_addr) & 0xFFF) {
        Log::error("intel-ppgtt: insert_range gpu_addr not page-aligned: 0x%llx", gfx_raw(gpu_addr));
        return false;
    }
    if (size & 0xFFF) {
        Log::error("intel-ppgtt: insert_range size not page-aligned: 0x%llx", static_cast<u64>(size));
        return false;
    }

    const usize page_count = size / PAGE_SIZE;
    const u64 phys_base = phys_raw(phys_start);

    for (usize page = 0; page < page_count; page++) {
        const gfx_addr_t page_gpu_addr = gfx_add(gpu_addr, page * PAGE_SIZE);
        const u64 page_phys = phys_base + page * PAGE_SIZE;

        const PpgttIndexer indexer(page_gpu_addr);

        gen_pte_t* pt = ensure_pt(indexer.pml4_i, indexer.pdpt_i, indexer.pd_i);
        if (!pt) {
            Log::error("intel-ppgtt: insert_range failed at page %u/%u", static_cast<u32>(page),
                      static_cast<u32>(page_count));
            return false;
        }

        pt[indexer.pt_i] = encode_leaf(page_phys, caching, writable);
    }

    return true;
}

}  // namespace gpu::intel::core