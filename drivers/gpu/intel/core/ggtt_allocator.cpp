// ggtt_allocator.cpp
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

#include "ggtt_allocator.h"

#include <pci/pci_device.h>
#include <pci/pci_host_bridge.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

namespace gpu::intel::core {

    namespace {

        struct GmadrInfo {
            u64 base;
            u64 size;
            bool valid;
        };

        [[nodiscard]] GmadrInfo read_gmadr(const volatile INTEL_IGP_PCI_CONFIG* cfg) {
            GMADR_0_2_0_PCI gmadr;
            gmadr.dwords.lo = cfg->gmadr_lo;
            gmadr.dwords.hi = cfg->gmadr_hi;

            if (gmadr.mem_io_space != 0 || gmadr.mem_type != 2 || gmadr.prefetchable != 1) {
                Log::error(
                    "ggtt: GMADR BAR2 flags unexpected "
                    "(mem_io=%u type=%u prefetch=%u) - not a valid 64-bit memory BAR",
                    gmadr.mem_io_space,
                    gmadr.mem_type,
                    gmadr.prefetchable
                );
                return {.base = 0, .size = 0, .valid = false};
            }

            const u64 base = gmadr.base_address();
            if (base == 0) {
                Log::error("ggtt: GMADR base is 0 - firmware did not configure BAR2");
                return {.base = 0, .size = 0, .valid = false};
            }

            MSAC_0_2_0_PCI msac;
            msac.raw = cfg->msac.raw;
            const u64 size = msac.aperture_size_bytes();

            Log::debug("ggtt: GMADR base=0x%016llx  MSAC raw=0x%02x  aperture=%llu MB", base, msac.raw, size >> 20);

            return {.base = base, .size = size, .valid = true};
        }

    }  // namespace

    bool GgttAllocator::init_from_device(
        volatile u8* mmio_base, const volatile INTEL_IGP_PCI_CONFIG* igp_cfg, const pci::pci_id& pci_id
    ) {
        mmio_base_ = mmio_base;

        const volatile pci::INTEL_HB_PCI_CONFIG* hb = pci::get_host_bridge(pci_id.domain);
        if (!hb) {
            Log::error("ggtt: host bridge not found for domain %u", pci_id.domain);
            return false;
        }

        if (hb->header.header.vendor_id != 0x8086) {
            Log::error("ggtt: unexpected host bridge vendor 0x%04x", hb->header.header.vendor_id);
            return false;
        }

        GGC_0_0_0_PCI ggc;
        ggc.raw = hb->ggc;

        Log::debug(
            "ggtt: GGC raw=0x%04x gms=0x%02x ggms=0x%x lock=%u ivd=%u",
            ggc.raw,
            ggc.gms,
            ggc.ggms,
            ggc.lock,
            ggc.ivd
        );

        const u64 dsm_bytes = ggc.dsm_size_bytes();
        const u64 gsm_bytes = ggc.gsm_size_bytes();
        const u32 gsm_entries = ggc.ggtt_entry_count();

        Log::info(
            "ggtt: DSM=%llu MB GSM=%llu MB GTT capacity=%u entries", dsm_bytes >> 20, gsm_bytes >> 20, gsm_entries
        );

        if (gsm_entries == 0) {
            Log::error("ggtt: GGMS=0, no GGTT space available");
            return false;
        }

        const GmadrInfo gmadr = read_gmadr(igp_cfg);
        if (!gmadr.valid) {
            Log::error("ggtt: could not determine aperture from GMADR/MSAC");
            return false;
        }

        Log::info("ggtt: GMADR base=0x%016llx aperture=%llu MB", gmadr.base, gmadr.size >> 20);

        const u32 aperture_entries = static_cast<u32>(gmadr.size / PAGE_SIZE);
        const u32 total_entries = (gsm_entries < aperture_entries) ? gsm_entries : aperture_entries;

        Log::debug(
            "ggtt: GSM entries=%u aperture entries=%u using=%u", gsm_entries, aperture_entries, total_entries
        );

        const u32 dsm_pages = static_cast<u32>(dsm_bytes / PAGE_SIZE);

        const u32 bdsm = hb->bdsm.address();
        const u32 bgsm = hb->bgsm.address();
        const u32 tolud = hb->tolud.address();

        u32 firmware_reserved = dsm_pages;

        if (bdsm != 0 && tolud != 0 && tolud > bdsm) {
            const u32 dsm_from_bars = (tolud - bdsm) / PAGE_SIZE;
            const u32 gsm_from_bars = (bgsm != 0 && bdsm > bgsm) ? (bdsm - bgsm) / PAGE_SIZE : 0;

            if (dsm_from_bars != dsm_pages) {
                Log::warning(
                    "ggtt: DSM mismatch GGC=%u pages TOLUD-BDSM=%u pages using larger", dsm_pages, dsm_from_bars
                );

                firmware_reserved = (dsm_from_bars > dsm_pages) ? dsm_from_bars : dsm_pages;
            }

            Log::debug(
                "ggtt: TOLUD=0x%08x BDSM=0x%08x BGSM=0x%08x DSM=%u pages GSM=%u pages",
                tolud,
                bdsm,
                bgsm,
                firmware_reserved,
                gsm_from_bars
            );
        } else {
            Log::warning("ggtt: BDSM/TOLUD not set, using GGC reservation (%u pages)", firmware_reserved);
        }

        Log::info(
            "ggtt: firmware reserved=%u entries (%lu MB)", firmware_reserved, (firmware_reserved * PAGE_SIZE) >> 20
        );

        if (total_entries <= firmware_reserved) {
            Log::error("ggtt: GGTT too small (%u) for firmware reservation (%u)", total_entries, firmware_reserved);
            return false;
        }

        gtt_entries_ = reinterpret_cast<volatile u64*>(mmio_base_ + GTT_OFFSET);

        init_index_space(total_entries, firmware_reserved);

        Log::info(
            "ggtt: ready total=%u reserved=%u usable=%u entries (%lu MB)",
            total_entries,
            firmware_reserved,
            total_entries - firmware_reserved,
            ((total_entries - firmware_reserved) * PAGE_SIZE) >> 20
        );

        return true;
    }

    void GgttAllocator::init_index_space(const u32 total_entries, const u32 start_index) {
        const u32 usable = total_entries - start_index;
        const u32 transient_sz = usable / GGTT_TRANSIENT_FRACTION;
        const u32 persistent_sz = usable - transient_sz;

        persistent_base_ = start_index;
        persistent_next_ = start_index;
        persistent_limit_ = start_index + persistent_sz;

        transient_base_ = persistent_limit_;
        transient_end_ = total_entries;

        // Register the entire transient region as one large free block.
        free_list_[0] = GgttBlock{
            .start_index = transient_base_,
            .num_pages = transient_sz,
            .in_use = false,
        };
        free_list_count_ = 1;

        lock_.init("ggtt_alloc");

        Log::log_dbc(
            "ggtt: persistent=[%u, %u] (%u pages), transient=[%u, %u] (%u pages)",
            persistent_base_,
            persistent_limit_,
            persistent_sz,
            transient_base_,
            transient_end_,
            transient_sz
        );
    }

    u32 GgttAllocator::index_alloc_persistent(const u32 num_pages) {
        SpinlockGuardIrq guard(lock_);

        if (persistent_next_ + num_pages > persistent_limit_) {
            Log::log_dbc("ggtt: persistent zone exhausted! (need %u pages)", num_pages);
            return U32_MAX;
        }

        const u32 index = persistent_next_;
        persistent_next_ += num_pages;

        Log::log_dbc("ggtt: persistent alloc %u pages -> index %u", num_pages, index);
        return index;
    }

    u32 GgttAllocator::index_alloc_transient(const u32 num_pages) {
        SpinlockGuardIrq guard(lock_);

        for (usize i = 0; i < free_list_count_; i++) {
            GgttBlock& block = free_list_[i];

            if (block.in_use || block.num_pages < num_pages) {
                continue;
            }

            const u32 alloc_index = block.start_index;

            if (block.num_pages == num_pages) {
                block.in_use = true;
            } else {
                if (free_list_count_ >= GGTT_MAX_FREE_BLOCKS) {
                    Log::log_dbc(
                        "ggtt: freelist full, wasting %u pages after index %u",
                        block.num_pages - num_pages,
                        alloc_index + num_pages
                    );
                    block.in_use = true;
                    block.num_pages = num_pages;
                } else {
                    const u32 remainder_start = block.start_index + num_pages;
                    const u32 remainder_pages = block.num_pages - num_pages;

                    block.start_index = alloc_index;
                    block.num_pages = num_pages;
                    block.in_use = true;

                    free_list_[free_list_count_++] = GgttBlock{
                        .start_index = remainder_start,
                        .num_pages = remainder_pages,
                        .in_use = false,
                    };
                }
            }

            return alloc_index;
        }

        return U32_MAX;
    }

    void GgttAllocator::index_free_transient(const u32 start_index) {
        SpinlockGuardIrq guard(lock_);

        const int idx = find_block(start_index);

        if (idx < 0) {
            Log::log_dbc("ggtt: free_transient called with unknown index %u", start_index);
            return;
        }

        GgttBlock& block = free_list_[idx];

        if (!block.in_use) {
            Log::log_dbc("ggtt: double-free detected at GTT index %u!", start_index);
            return;
        }

        block.in_use = false;
        Log::log_dbc("ggtt: freed %u pages at index %u", block.num_pages, start_index);

        // Coalesce after every free to keep the freelist compact and prevent
        // fragmentation under high-frequency small allocations.
        coalesce();
    }

    void GgttAllocator::coalesce() {
        bool merged = true;
        while (merged) {
            merged = false;

            for (usize i = 0; i < free_list_count_ && !merged; i++) {
                if (free_list_[i].in_use) continue;

                for (usize j = 0; j < free_list_count_ && !merged; j++) {
                    if (i == j || free_list_[j].in_use) continue;

                    GgttBlock& a = free_list_[i];
                    GgttBlock& b = free_list_[j];

                    const bool a_before_b = (a.start_index + a.num_pages == b.start_index);
                    const bool b_before_a = (b.start_index + b.num_pages == a.start_index);

                    if (a_before_b || b_before_a) {
                        // The block with the lower start_index absorbs the other.
                        const u32 merged_start = a_before_b ? a.start_index : b.start_index;
                        const u32 merged_pages = a.num_pages + b.num_pages;

                        a.start_index = merged_start;
                        a.num_pages = merged_pages;
                        a.in_use = false;

                        // Remove B by swapping it with the last entry.
                        free_list_[j] = free_list_[--free_list_count_];

                        merged = true;
                    }
                }
            }
        }
    }

    int GgttAllocator::find_block(const u32 start_index) const {
        for (usize i = 0; i < free_list_count_; i++) {
            if (free_list_[i].start_index == start_index) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    u32 GgttAllocator::transient_free_pages() const {
        u32 free = 0;
        for (usize i = 0; i < free_list_count_; i++) {
            if (!free_list_[i].in_use) {
                free += free_list_[i].num_pages;
            }
        }
        return free;
    }

    void GgttAllocator::write_entries(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const {
        for (usize i = 0; i < num_pages; i++) {
            const u64 page_phys = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));

            u64 gtt_entry = page_phys & GTT_PHYS_ADDR_MASK;
            gtt_entry |= (static_cast<u64>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
            gtt_entry |= GTT_VALID;

            gtt_entries_[gtt_index + i] = gtt_entry;
        }
        asm volatile("mfence" ::: "memory");
    }

    void GgttAllocator::clear_entries(u32 gtt_index, usize num_pages) const {
        for (usize i = 0; i < num_pages; i++) {
            gtt_entries_[gtt_index + i] = 0;
        }
        asm volatile("mfence" ::: "memory");
    }

    GgttAllocation GgttAllocator::alloc_persistent(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);

        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags | (1ULL << PtFlag::ReadWrite));

        const u32 gtt_index = index_alloc_persistent(static_cast<u32>(num_pages));

        if (gtt_index == U32_MAX) {
            Log::log_dbc("ggtt: alloc_persistent: persistent zone exhausted");
            return {};
        }

        write_entries(gtt_index, phys, num_pages, pat_index);

        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    GgttAllocation GgttAllocator::alloc_transient(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);

        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags | (1ULL << PtFlag::ReadWrite));

        const u32 gtt_index = index_alloc_transient(static_cast<u32>(num_pages));

        if (gtt_index == U32_MAX) {
            Log::log_dbc("ggtt: alloc_transient: transient zone exhausted");
            kernel::memory::free_pages_phys(phys, num_pages);
            return {};
        }

        write_entries(gtt_index, phys, num_pages, pat_index);

        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    void GgttAllocator::free_transient(const GgttAllocation& alloc, usize num_pages) {
        if (virt_null(alloc.cpu_addr)) {
            return;
        }

        const u32 gtt_index = static_cast<u32>(gfx_raw(alloc.gfx_addr) / PAGE_SIZE);

        clear_entries(gtt_index, num_pages);
        index_free_transient(gtt_index);

        const phys_addr_t phys = virt_to_phys(alloc.cpu_addr);

        kernel::memory::free_pages_phys(phys, num_pages);
    }

}  // namespace blt
