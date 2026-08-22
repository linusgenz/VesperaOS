// ggtt_allocator.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.03.26.
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
#ifndef VESPERAOS_GGTT_ALLOCATOR_H
#define VESPERAOS_GGTT_ALLOCATOR_H

#include <pci/pci_id.h>
#include <vespera/mm/addr.h>
#include <vespera/sync/spinlock.h>
#include <vespera/types.h>

#include <gpu/intel/regs/pci_config_regs.h>

#include "vespera/mm/memory.h"

namespace gpu::intel::core {

    // GTT / GGTT — PTE format, shared by every engine that allocates GGTT-backed
    // memory (BCS, RCS, ...). There is exactly one GGTT per physical GPU, owned by
    // IntelGpuDevice, and every engine borrows it through IntelEngine::ggtt().

    constexpr usize GTT_OFFSET = 8ull * 1024 * 1024;
    constexpr u32 GTT_TOTAL_ENTRIES = 256u * 1024u;

    constexpr u64 GTT_VALID = 0x01ULL;
    constexpr u64 GTT_PHYS_ADDR_MASK = 0x000FFFFFFFFFF000ULL;
    constexpr u32 GTT_PAT_SHIFT = 2;
    constexpr u64 GTT_PAT_MASK = 0x3ULL;

    // PAT memory types
    constexpr u8 GTT_PAT_UC = 0x0;
    constexpr u8 GTT_PAT_WC = 0x1;
    constexpr u8 GTT_PAT_WT = 0x2;
    constexpr u8 GTT_PAT_WB = 0x3;

    // MOCS indices
    constexpr u8 MOCS_UNCACHED = 0x01;
    constexpr u8 MOCS_LLC_ONLY = 0x02;
    constexpr u8 MOCS_DISPLAY_BUFFER = 0x03;
    constexpr u8 MOCS_CACHED_WB = 0x09;

    static constexpr usize GGTT_MAX_FREE_BLOCKS = 64;

    // Fraction of the usable GGTT reserved for transient allocations (1/4).
    static constexpr usize GGTT_TRANSIENT_FRACTION = 4;

    struct GgttAllocation {
        virt_addr_t cpu_addr{};
        gfx_addr_t gfx_addr{};
    };

    struct GgttBlock {
        u32 start_index;  // GTT page index of the first page in this block
        u32 num_pages;    // Size of the block in pages
        bool in_use;      // true = allocated, false = free
    };

    /**
     * @brief Owns the device's single GGTT (Global Graphics Translation Table).
     *
     * There is exactly one GGTT per GPU — every engine (BCS, RCS, ...) must
     * allocate out of the same instance, or their GPU address spaces can
     * collide. IntelGpuDevice owns one instance and hands out a reference to
     * every engine constructed on top of it (see IntelEngine::ggtt()).
     */
    class GgttAllocator {
       public:
        GgttAllocator() = default;

        /**
         * @brief One-time device-level GGTT setup.
         *
         * @return false if the host bridge, GMADR, or MSAC could not be
         *         resolved, or if the resulting GGTT is too small for the
         *         firmware's stolen-memory reservation.
         */
        [[nodiscard]] bool init_from_device(
            volatile u8* mmio_base, const volatile INTEL_IGP_PCI_CONFIG* igp_cfg, const pci::pci_id& pci_id
        );

        [[nodiscard]] GgttAllocation alloc_persistent(usize num_pages, u64 flags = 0, u8 pat_index = GTT_PAT_UC);

        [[nodiscard]] GgttAllocation alloc_transient(usize num_pages, u64 flags, u8 pat_index);

        void free_transient(const GgttAllocation& alloc, usize num_pages);

        [[nodiscard]] u32 persistent_used_pages() const {
            return persistent_next_ - persistent_base_;
        }
        [[nodiscard]] u32 transient_total_pages() const {
            return transient_end_ - transient_base_;
        }
        [[nodiscard]] u32 transient_free_pages() const;
        [[nodiscard]] u32 transient_used_pages() const {
            return transient_total_pages() - transient_free_pages();
        }

        [[nodiscard]] u32 usable_pages() const {
            return transient_end_ - persistent_base_;
        }

        [[nodiscard]] u64 usable_size_bytes() const {
            return static_cast<u64>(usable_pages()) * PAGE_SIZE;
        }

       private:
        void init_index_space(u32 total_entries, u32 start_index);

        [[nodiscard]] u32 index_alloc_persistent(u32 num_pages);
        [[nodiscard]] u32 index_alloc_transient(u32 num_pages);
        void index_free_transient(u32 start_index);

        void coalesce();
        [[nodiscard]] int find_block(u32 start_index) const;

        void write_entries(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const;
        void clear_entries(u32 gtt_index, usize num_pages) const;

        volatile u8* mmio_base_ = nullptr;
        volatile u64* gtt_entries_ = nullptr;

        u32 persistent_base_ = 0;
        u32 persistent_next_ = 0;   // next free page index (bump pointer)
        u32 persistent_limit_ = 0;  // exclusive upper bound of the persistent zone

        u32 transient_base_ = 0;
        u32 transient_end_ = 0;

        GgttBlock free_list_[GGTT_MAX_FREE_BLOCKS] = {};
        usize free_list_count_ = 0;

        mutable Spinlock lock_;
    };

}  // namespace gpu::intel::core

#endif  // VESPERAOS_GGTT_ALLOCATOR_H
