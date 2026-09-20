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
    } // namespace

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

    void IntelPpgtt::record_table_page(const GgttAllocation& alloc) {
        if (table_page_count_ >= MAX_TABLE_PAGES) {
            Log::error("intel-ppgtt: table_pages_ full, cannot record phys=0x%llx ggtt=0x%llx",
                       phys_raw(alloc.phys_addr), gfx_raw(alloc.gfx_addr));
            return;
        }
        table_pages_[table_page_count_++] = TablePage{alloc};
    }

    u64 IntelPpgtt::ggtt_for_phys(const u64 phys_addr) const {
        for (usize i = 0; i < table_page_count_; i++) {
            if (phys_raw(table_pages_[i].alloc.phys_addr) == phys_addr) {
                return gfx_raw(table_pages_[i].alloc.gfx_addr);
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
        auto scratch_page = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(scratch_page.cpu_addr)) {
            Log::error("intel-ppgtt: scratch page allocation failed");
            return false;
        }
        memset(virt_ptr(scratch_page.cpu_addr), 0, PAGE_SIZE);
        scratch_page_phys_addr_ = phys_raw(scratch_page.phys_addr);
        scratch_page_alloc_ = scratch_page;
        // The scratch page is only ever used as a PTE leaf target, never as a
        // parent whose entries we need to CPU-edit later, so it doesn't need a
        // table_pages_ entry -- destroy() frees scratch_page_alloc_ directly
        // instead.

        // Scratch PT: every entry points at the scratch page.
        auto scratch_pt = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(scratch_pt.cpu_addr)) {
            Log::error("intel-ppgtt: scratch PT allocation failed");
            return false;
        }
        scratch_pt_phys_addr_ = phys_raw(scratch_pt.phys_addr);
        scratch_pt_alloc_ = scratch_pt;
        record_table_page(scratch_pt);
        {
            auto* entries = static_cast<gen_pte_t*>(virt_ptr(scratch_pt.cpu_addr));
            const gen_pte_t leaf = encode_leaf(scratch_page_phys_addr_, PpgttCaching::NONE, false);
            for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
                entries[i] = leaf;
            }
        }

        // Scratch PD: every entry points at the scratch PT.
        auto scratch_pd = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(scratch_pd.cpu_addr)) {
            Log::error("intel-ppgtt: scratch PD allocation failed");
            return false;
        }
        scratch_pd_phys_addr_ = phys_raw(scratch_pd.phys_addr);
        scratch_pd_alloc_ = scratch_pd;
        record_table_page(scratch_pd);
        {
            auto* entries = static_cast<gen_pte_t*>(virt_ptr(scratch_pd.cpu_addr));
            const gen_pte_t ptr = encode_table_pointer(scratch_pt_phys_addr_);
            for (usize i = 0; i < PPGTT_ENTRIES_PER_TABLE; i++) {
                entries[i] = ptr;
            }
        }

        // Scratch PDPT: every entry points at the scratch PD.
        auto scratch_pdpt = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(scratch_pdpt.cpu_addr)) {
            Log::error("intel-ppgtt: scratch PDPT allocation failed");
            return false;
        }
        scratch_pdpt_phys_addr_ = phys_raw(scratch_pdpt.phys_addr);
        scratch_pdpt_alloc_ = scratch_pdpt;
        record_table_page(scratch_pdpt);
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
        auto pml4 = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
        if (virt_null(pml4.cpu_addr)) {
            Log::error("intel-ppgtt: PML4 allocation failed");
            return false;
        }
        pml4_phys_addr_ = phys_raw(pml4.phys_addr);
        pml4_virt_ = static_cast<gen_pte_t*>(virt_ptr(pml4.cpu_addr));
        pml4_alloc_ = pml4;
        // PML4 is the root - nothing ever reads a "parent" entry pointing at it,
        // so no table_pages_ entry is needed for it either -- destroy() frees
        // pml4_alloc_ directly instead.

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
            auto alloc = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
            if (virt_null(alloc.cpu_addr)) {
                Log::error("intel-ppgtt: PDPT allocation failed (pml4_i=%u)", pml4_i);
                return nullptr;
            }

            pdpt_phys_addr = phys_raw(alloc.phys_addr);
            record_table_page(alloc);

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
            auto alloc = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
            if (virt_null(alloc.cpu_addr)) {
                Log::error("intel-ppgtt: PD allocation failed (pml4_i=%u pdpt_i=%u)", pml4_i, pdpt_i);
                return nullptr;
            }

            pd_phys_addr = phys_raw(alloc.phys_addr);
            record_table_page(alloc);

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
            auto alloc = ggtt_.alloc_transient(1, (1ULL << CacheDisabled), MOCS_UNCACHED);
            if (virt_null(alloc.cpu_addr)) {
                Log::error("intel-ppgtt: PT allocation failed (pml4_i=%u pdpt_i=%u pd_i=%u)", pml4_i, pdpt_i, pd_i);
                return nullptr;
            }

            pt_phys_addr = phys_raw(alloc.phys_addr);
            record_table_page(alloc);

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

    bool IntelPpgtt::insert_range(
        const gfx_addr_t gpu_addr, const phys_addr_t phys_start, const usize size,
        const PpgttCaching caching, const bool writable
    ) {
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

    void* IntelPpgtt::gpu_to_virt(const gfx_addr_t gpu_addr) const {
        const PpgttIndexer indexer(gpu_addr);

        u64 pdpt_phys = pml4_virt_[indexer.pml4_i] & ~static_cast<u64>(0xFFF);
        u64 pdpt_ggtt = ggtt_for_phys(pdpt_phys);
        if (pdpt_ggtt == 0) return nullptr;
        auto* pdpt = table_virt(pdpt_ggtt);

        u64 pd_phys = pdpt[indexer.pdpt_i] & ~static_cast<u64>(0xFFF);
        u64 pd_ggtt = ggtt_for_phys(pd_phys);
        if (pd_ggtt == 0) return nullptr;
        auto* pd = table_virt(pd_ggtt);

        u64 pt_phys = pd[indexer.pd_i] & ~static_cast<u64>(0xFFF);
        u64 pt_ggtt = ggtt_for_phys(pt_phys);
        if (pt_ggtt == 0) return nullptr;
        auto* pt = table_virt(pt_ggtt);

        const u64 page_phys = pt[indexer.pt_i] & ~static_cast<u64>(0xFFF);
        const u64 page_off = gfx_raw(gpu_addr) & 0xFFF;

        return static_cast<u8*>(virt_ptr(phys_to_virt(make_phys(page_phys)))) + page_off;
    }

    void IntelPpgtt::destroy() {
        for (usize i = 0; i < table_page_count_; i++) {
            ggtt_.free_transient(table_pages_[i].alloc, 1);
        }
        table_page_count_ = 0;

        if (!virt_null(scratch_page_alloc_.cpu_addr)) {
            ggtt_.free_transient(scratch_page_alloc_, 1);
        }
        if (!virt_null(pml4_alloc_.cpu_addr)) {
            ggtt_.free_transient(pml4_alloc_, 1);
        }

        for (auto & table_page : table_pages_) {
            table_page = TablePage{};
        }

        scratch_page_alloc_ = GgttAllocation{};
        scratch_pt_alloc_ = GgttAllocation{};
        scratch_pd_alloc_ = GgttAllocation{};
        scratch_pdpt_alloc_ = GgttAllocation{};
        pml4_alloc_ = GgttAllocation{};

        scratch_page_phys_addr_ = 0;
        scratch_pt_phys_addr_ = 0;
        scratch_pd_phys_addr_ = 0;
        scratch_pdpt_phys_addr_ = 0;

        pml4_phys_addr_ = 0;
        pml4_virt_ = nullptr;
    }

    void IntelPpgtt::dump_batch_buffer(const gfx_addr_t batch_addr, const u32 batch_len) const {
        Log::log_dbc("intel-ppgtt: batch dump addr=0x%llx len=%u", gfx_raw(batch_addr), batch_len);

        const u32 dword_count = batch_len / sizeof(u32);
        for (u32 i = 0; i < dword_count; i += 4) {
            const gfx_addr_t line_addr = gfx_add(batch_addr, i * sizeof(u32));

            // Batch kann über mehrere Pages laufen -> pro Dword einzeln walken,
            // oder (schneller) nur bei Page-Grenzübertritt neu walken.
            const u32* w0 = static_cast<const u32*>(gpu_to_virt(line_addr));
            const u32 v0 = w0 ? *w0 : 0xDEADBEEF;
            const u32 v1 = (i + 1 < dword_count)
                               ? *(static_cast<const u32*>(gpu_to_virt(gfx_add(batch_addr, (i + 1) * 4))))
                               : 0;
            const u32 v2 = (i + 2 < dword_count)
                               ? *(static_cast<const u32*>(gpu_to_virt(gfx_add(batch_addr, (i + 2) * 4))))
                               : 0;
            const u32 v3 = (i + 3 < dword_count)
                               ? *(static_cast<const u32*>(gpu_to_virt(gfx_add(batch_addr, (i + 3) * 4))))
                               : 0;

            Log::log_dbc("  [0x%04x] %08x %08x %08x %08x", i * 4, v0, v1, v2, v3);
        }
    }
} // namespace gpu::intel::core
