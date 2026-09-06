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

// Ref: https://kiwitree.net/~lina/intel-gfx-docs/prm/kbl/intel-gfx-prm-osrc-kbl-vol05-memory_views.pdf (Chapter "Graphics Translation Tables")

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
    constexpr usize PPGTT_PT_COVERAGE = 4096ull * PPGTT_ENTRIES_PER_TABLE;             // 2MB
    constexpr usize PPGTT_PD_COVERAGE = PPGTT_PT_COVERAGE * PPGTT_ENTRIES_PER_TABLE;   // 1GB
    constexpr usize PPGTT_PDPT_COVERAGE = PPGTT_PD_COVERAGE * PPGTT_ENTRIES_PER_TABLE; // 512GB
    constexpr usize PPGTT_VA_SPACE_SIZE = PPGTT_PDPT_COVERAGE * PPGTT_ENTRIES_PER_TABLE; // 2^48 = 256TB
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
        LLC  = 4,
    };

    class IntelPpgtt {
    public:
        explicit IntelPpgtt(GgttAllocator& ggtt) : ggtt_(ggtt) {
        }

        ~IntelPpgtt() = default;

        IntelPpgtt(const IntelPpgtt&) = delete;
        IntelPpgtt& operator=(const IntelPpgtt&) = delete;
        [[nodiscard]] bool init();

        [[nodiscard]] bool insert_range(
            gfx_addr_t gpu_addr, phys_addr_t phys_start, usize size,
            PpgttCaching caching, bool writable
        );

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

        u64 scratch_page_phys_addr_ = 0;
        u64 scratch_pt_phys_addr_ = 0;
        u64 scratch_pd_phys_addr_ = 0;
        u64 scratch_pdpt_phys_addr_ = 0;

        u64 pml4_phys_addr_ = 0;
        gen_pte_t* pml4_virt_ = nullptr;
    };
} // namespace gpu::intel::core

#endif  // VESPERAOS_GPU_INTEL_CORE_INTEL_PPGTT_H
