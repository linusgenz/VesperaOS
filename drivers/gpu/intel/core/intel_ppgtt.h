// intel_ppgtt.h
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

#ifndef VESPERAOS_GPU_INTEL_CORE_INTEL_PPGTT_H
#define VESPERAOS_GPU_INTEL_CORE_INTEL_PPGTT_H

#include <vespera/types.h>
#include "vespera/mm/addr.h"

#include "ggtt_allocator.h"

namespace gpu::intel::core {

// Gen9 48-bit canonical PPGTT layout (matches CONTEXT_DESCRIPTOR::LEGACY_64BIT_PPGTT):
//   PML4 (1 table, 512 entries, 512GB each)
//    -> PDPT (512 entries, 1GB each)
//        -> PD (512 entries, 2MB each)
//            -> PT (512 entries, 4KB each)
//
// All four levels are single 4KB pages allocated via the same
// ggtt().alloc_persistent() path used for the ring/LRC/HWSP elsewhere (a
// convenient, already-phys-contiguous, already-CPU-mapped source - not
// because the tables need to live in the GGTT aperture). The GPU walks this
// structure exclusively through physical/bus addresses in every PTE/PDE/
// PDPE/PML4E, never through GGTT offsets; GGTT mappings for these pages are
// only used CPU-side, to get a writable pointer for editing table entries.
// This is a distinct table format from kernel::memory::PageTableManager (CPU
// MMU paging); the two share no code.
constexpr usize PPGTT_ENTRIES_PER_TABLE = 512;
constexpr usize PPGTT_PT_COVERAGE = 4096ull * PPGTT_ENTRIES_PER_TABLE;                    // 2MB
constexpr usize PPGTT_PD_COVERAGE = PPGTT_PT_COVERAGE * PPGTT_ENTRIES_PER_TABLE;          // 1GB
constexpr usize PPGTT_PDPT_COVERAGE = PPGTT_PD_COVERAGE * PPGTT_ENTRIES_PER_TABLE;        // 512GB

using gen_pte_t = u64;

constexpr u64 PPGTT_PAGE_PRESENT = 1ull << 0;
constexpr u64 PPGTT_PAGE_RW = 1ull << 1;
constexpr u64 PPGTT_PAGE_PWT = 1ull << 3;
constexpr u64 PPGTT_PAGE_PCD = 1ull << 4;
constexpr u64 PPGTT_PAGE_PAT = 1ull << 7;

// PAT index selection mirrors InitPrivatePat()'s table: index 3 = uncacheable,
// index 4 = LLC write-back. Only these two are used until display/media caching
// modes are needed.
enum class PpgttCaching : u32 {
    NONE = 3,
    LLC = 4,
};

// One 4KB, zero-initialized, GGTT-mapped page used as every table's default
// leaf/child target. Every PT entry starts pointed at kScratchPage (present,
// read-only) instead of being left absent, so a stray GPU access into an
// unmapped range reads zero instead of walking off into an unrelated
// allocation or faulting the context (see CONTEXT_DESCRIPTOR::FAULT_AND_HANG -
// legacy context mode only supports fault-and-hang, so bring-up code should
// avoid triggering faults at all where avoidable).
//
// Address kinds used throughout this class:
//   - "phys" addresses go into every PTE/PDE/PDPE/PML4E as the pointer to the
//     child table or leaf page. The GPU walks the PPGTT exclusively through
//     these - never through GGTT offsets.
//   - "ggtt" addresses are only used CPU-side, via table_virt(), to get a
//     writable pointer to a table page so we can edit its entries.
// Every table-level allocation therefore carries both, and we keep a small
// linear phys->ggtt lookup (below) so that when we read a child pointer back
// out of a parent entry (which is now a phys address), we can still find the
// GGTT mapping needed to CPU-write into that child.
class IntelPpgtt {
public:
    explicit IntelPpgtt(GgttAllocator& ggtt) : ggtt_(ggtt) {}
    ~IntelPpgtt() = default;

    IntelPpgtt(const IntelPpgtt&) = delete;
    IntelPpgtt& operator=(const IntelPpgtt&) = delete;

    // Allocates the scratch page, scratch PT/PD/PDPT chain, and the root PML4
    // table, then points every PML4 entry at the scratch PDPT. Must be called
    // once before any insert_range()/pml4_phys_addr() call.
    [[nodiscard]] bool init();

    // Maps [gpu_addr, gpu_addr + size) to the physical pages backing
    // cpu_addr..cpu_addr+size (same phys-contiguity assumption as
    // ggtt().alloc_persistent() results - callers pass GGTT allocations
    // through here, which are always phys-contiguous). Allocates any PT/PD/
    // PDPT levels not yet present. gpu_addr and size must be page-aligned.
    [[nodiscard]] bool insert_range(gfx_addr_t gpu_addr, phys_addr_t phys_start, usize size,
                                    PpgttCaching caching, bool writable);

    // PML4 table's physical address, in raw byte form - this is what gets
    // written into PDP0_DESCRIPTOR (LRC_DW_PDP0_UDW/LDW) for
    // CONTEXT_DESCRIPTOR::LEGACY_64BIT_PPGTT submission. The GPU page-table
    // walker resolves PDP0 as a physical/bus address, never a GGTT offset.
    [[nodiscard]] u64 pml4_phys_addr_bytes() const {
        return pml4_phys_addr_;
    }

private:
    GgttAllocator& ggtt_;

    struct PpgttIndexer {
        explicit PpgttIndexer(gfx_addr_t addr);

        u32 pml4_i;
        u32 pdpt_i;
        u32 pd_i;
        u32 pt_i;
    };

    // Every table page (PDPT/PD/PT) we ever allocate is recorded here so that
    // ensure_pt() can turn the phys address stored in a parent entry back
    // into the GGTT address needed for CPU access via table_virt(). Tables
    // are bump-allocated and never freed, so a flat linear scan is fine for
    // bring-up; revisit if PPGTT teardown/reuse is ever added.
    struct TablePage {
        u64 phys_addr;
        u64 ggtt_addr;
    };
    static constexpr usize MAX_TABLE_PAGES = 4096;
    TablePage table_pages_[MAX_TABLE_PAGES] = {};
    usize table_page_count_ = 0;

    // Records a freshly allocated table page's phys/GGTT pair. Logs and drops
    // the mapping if the table is full (bring-up limit, see MAX_TABLE_PAGES).
    void record_table_page(u64 phys_addr, u64 ggtt_addr);

    // Looks up the GGTT address for a table page previously recorded via
    // record_table_page(), given its phys address (as read back out of a
    // parent PTE/PDE/PDPE). Returns 0 (invalid) if not found.
    [[nodiscard]] u64 ggtt_for_phys(u64 phys_addr) const;

    [[nodiscard]] bool alloc_scratch_chain();
    [[nodiscard]] bool alloc_root();

    // Returns the PT for (pml4_i, pdpt_i, pd_i), allocating any missing PDPT/
    // PD/PT level along the way. Returns nullptr on allocation failure.
    [[nodiscard]] gen_pte_t* ensure_pt(u32 pml4_i, u32 pdpt_i, u32 pd_i);

    [[nodiscard]] gen_pte_t* table_virt(u64 ggtt_addr) const;

    // Scratch chain phys addresses - these are what get written into PTE/PDE/
    // PDPE entries pointing at scratch. The corresponding GGTT addresses (for
    // CPU access via table_virt()) aren't kept as members: they're only
    // needed once, right after allocation in alloc_scratch_chain(), and are
    // otherwise recoverable via ggtt_for_phys() like any other table page.
    u64 scratch_page_phys_addr_ = 0;
    u64 scratch_pt_phys_addr_ = 0;
    u64 scratch_pd_phys_addr_ = 0;
    u64 scratch_pdpt_phys_addr_ = 0;

    u64 pml4_phys_addr_ = 0;
    gen_pte_t* pml4_virt_ = nullptr;
};

}  // namespace gpu::intel::core

#endif  // VESPERAOS_GPU_INTEL_CORE_INTEL_PPGTT_H
