// intel_blt_ggtt.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.05.26.
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

#include <pci/pci_device.h>
#include <pci/pci_host_bridge.h>
#include <vespera/log.h>
#include <vespera/mm/memory.h>

#include "intel_blt.h"

namespace blt {
    [[nodiscard]] static GmadrInfo read_gmadr(const volatile INTEL_IGP_PCI_CONFIG* cfg) {
        GMADR_0_2_0_PCI gmadr;
        gmadr.dwords.lo = cfg->gmadr_lo;
        gmadr.dwords.hi = cfg->gmadr_hi;

        if (gmadr.mem_io_space != 0 || gmadr.mem_type != 2 || gmadr.prefetchable != 1) {
            Log::error(
                "intel-blt: GMADR BAR2 flags unexpected "
                "(mem_io=%u type=%u prefetch=%u) - not a valid 64-bit memory BAR",
                gmadr.mem_io_space,
                gmadr.mem_type,
                gmadr.prefetchable
            );
            return {.base = 0, .size = 0, .valid = false};
        }

        const u64 base = gmadr.base_address();
        if (base == 0) {
            Log::error("intel-blt: GMADR base is 0 - firmware did not configure BAR2");
            return {.base = 0, .size = 0, .valid = false};
        }

        MSAC_0_2_0_PCI msac;
        msac.raw = cfg->msac.raw;

        const u64 size = msac.aperture_size_bytes();

        Log::debug("intel-blt: GMADR base=0x%016llx  MSAC raw=0x%02x  aperture=%llu MB", base, msac.raw, size >> 20);

        return {.base = base, .size = size, .valid = true};
    }

    void IntelBlt::ggtt_init() {
        const volatile pci::INTEL_HB_PCI_CONFIG* hb = pci::get_host_bridge(pci_id_.domain);
        if (!hb) {
            Log::error("intel-blt: host bridge not found for domain %u", pci_id_.domain);
            return;
        }

        if (hb->header.header.vendor_id != 0x8086) {
            Log::error("intel-blt: unexpected host bridge vendor 0x%04x", hb->header.header.vendor_id);
            return;
        }

        GGC_0_0_0_PCI ggc;
        ggc.raw = hb->ggc;

        Log::debug(
            "intel-blt: GGC raw=0x%04x gms=0x%02x ggms=0x%x lock=%u ivd=%u",
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
            "intel-blt: DSM=%llu MB GSM=%llu MB GTT capacity=%u entries", dsm_bytes >> 20, gsm_bytes >> 20, gsm_entries
        );

        if (gsm_entries == 0) {
            Log::error("intel-blt: GGMS=0, no GGTT space available");
            return;
        }

        const GmadrInfo gmadr = read_gmadr(igp_cfg_);
        if (!gmadr.valid) {
            Log::error("intel-blt: could not determine aperture from GMADR/MSAC");
            return;
        }

        Log::info("intel-blt: GMADR base=0x%016llx aperture=%llu MB", gmadr.base, gmadr.size >> 20);

        const u32 aperture_entries = static_cast<u32>(gmadr.size / PAGE_SIZE);
        const u32 total_entries = (gsm_entries < aperture_entries) ? gsm_entries : aperture_entries;

        Log::debug(
            "intel-blt: GSM entries=%u aperture entries=%u using=%u", gsm_entries, aperture_entries, total_entries
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
                    "intel-blt: DSM mismatch GGC=%u pages TOLUD-BDSM=%u pages using larger", dsm_pages, dsm_from_bars
                );

                firmware_reserved = (dsm_from_bars > dsm_pages) ? dsm_from_bars : dsm_pages;
            }

            Log::debug(
                "intel-blt: TOLUD=0x%08x BDSM=0x%08x BGSM=0x%08x DSM=%u pages GSM=%u pages",
                tolud,
                bdsm,
                bgsm,
                firmware_reserved,
                gsm_from_bars
            );
        } else {
            Log::warning("intel-blt: BDSM/TOLUD not set using GGC reservation (%u pages)", firmware_reserved);
        }

        Log::info(
            "intel-blt: firmware reserved=%u entries (%lu MB)", firmware_reserved, (firmware_reserved * PAGE_SIZE) >> 20
        );

        if (total_entries <= firmware_reserved) {
            Log::error(
                "intel-blt: GGTT too small (%u) for firmware reservation (%u)", total_entries, firmware_reserved
            );
            return;
        }

        gtt_entries_ = reinterpret_cast<volatile u64*>(mmio_base_ + GTT_OFFSET);

        ggtt_alloc_.init(total_entries, firmware_reserved);

        Log::info(
            "intel-blt: GGTT ready total=%u reserved=%u usable=%u entries (%lu MB)",
            total_entries,
            firmware_reserved,
            total_entries - firmware_reserved,
            ((total_entries - firmware_reserved) * PAGE_SIZE) >> 20
        );
    }

    void IntelBlt::ggtt_write_entries(u32 gtt_index, phys_addr_t phys_addr, usize num_pages, u8 pat_index) const {
        for (usize i = 0; i < num_pages; i++) {
            const u64 page_phys = phys_raw(phys_add(phys_addr, i * PAGE_SIZE));

            u64 gtt_entry = page_phys & GTT_PHYS_ADDR_MASK;
            gtt_entry |= (static_cast<u64>(pat_index) & GTT_PAT_MASK) << GTT_PAT_SHIFT;
            gtt_entry |= GTT_VALID;

            gtt_entries_[gtt_index + i] = gtt_entry;

            asm volatile("mfence" ::: "memory");
        }
    }

    void IntelBlt::ggtt_clear_entries(u32 gtt_index, usize num_pages) const {
        for (usize i = 0; i < num_pages; i++) {
            gtt_entries_[gtt_index + i] = 0;
            asm volatile("mfence" ::: "memory");
        }
    }

    GgttAllocation IntelBlt::ggtt_alloc_persistent(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);

        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_persistent(static_cast<u32>(num_pages));

        if (gtt_index == U32_MAX) {
            Log::log_dbc("ggtt_alloc_persistent: persistent zone exhausted");
            error_count_++;
            return {};
        }

        ggtt_write_entries(gtt_index, phys, num_pages, pat_index);

        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    GgttAllocation IntelBlt::ggtt_alloc_transient(usize num_pages, u64 flags, u8 pat_index) {
        const phys_addr_t phys = kernel::memory::request_pages_phys(num_pages);
        const virt_addr_t cpu = phys_to_virt(phys);

        kernel::memory::map_range(cpu, phys, num_pages * PAGE_SIZE, flags);

        const u32 gtt_index = ggtt_alloc_.alloc_transient(static_cast<u32>(num_pages));

        if (gtt_index == U32_MAX) {
            Log::log_dbc("ggtt_alloc_transient: transient zone exhausted");
            kernel::memory::free_pages_phys(phys, num_pages);
            return {};
        }

        ggtt_write_entries(gtt_index, phys, num_pages, pat_index);

        return {cpu, make_gfx(static_cast<u64>(gtt_index) * PAGE_SIZE)};
    }

    void IntelBlt::ggtt_free_transient(const GgttAllocation& alloc, usize num_pages) {
        if (virt_null(alloc.cpu_addr)) {
            return;
        }

        const u32 gtt_index = static_cast<u32>(gfx_raw(alloc.gfx_addr) / PAGE_SIZE);

        ggtt_clear_entries(gtt_index, num_pages);

        ggtt_alloc_.free_transient(gtt_index);

        const phys_addr_t phys = virt_to_phys(alloc.cpu_addr);

        kernel::memory::free_pages_phys(phys, num_pages);
    }
}  // namespace blt