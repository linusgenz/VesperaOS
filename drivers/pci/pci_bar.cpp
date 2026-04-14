// pci_bar.cpp
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.04.26.
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

#include "pci_bar.h"

#include "pci.h"

namespace pci::bar {

    bool is_64_bit(const u32 bar_value) {
        if (bar_value & PCI_BAR_MEMORY_MASK)
            return false;

        const u32 type = (bar_value >> 1) & 0x3u;
        return type == 0x2u;
    }

    BarInfo read(PCI_HEADER0* header, const u8 index) {
        BarInfo info = {};

        if (index > 5)
            return info; // is_valid remains false

        volatile u32* bars = &header->bar0;
        const u32 raw = bars[index];

        if (raw == 0)
            return info; // BAR not implemented

        info.is_valid  = true;
        info.is_memory = !(raw & PCI_BAR_MEMORY_MASK);

        if (!info.is_memory) {
            // I/O BAR
            info.address    = raw & ~0x3ULL;
            info.is_64_bit  = false;
            info.is_prefetchable = false;

            const u32 orig = bars[index];
            bars[index] = 0xFFFFFFFFu;
            const u32 mask = bars[index] & ~0x3u;
            bars[index] = orig;
            info.size = ~mask + 1u;

        } else if (is_64_bit(raw)) {
            if (index >= 5) {
                // No room for the high DWORD
                info.is_valid = false;
                return info;
            }

            info.is_64_bit       = true;
            info.is_prefetchable = (raw >> 3) & 1u;

            const u32 raw_hi = bars[index + 1];
            info.address = (static_cast<u64>(raw_hi) << 32) | (raw & ~0xFULL);

            const u32 orig_lo = bars[index];
            const u32 orig_hi = bars[index + 1];

            bars[index]     = 0xFFFFFFFFu;
            bars[index + 1] = 0xFFFFFFFFu;

            const u32 size_lo = bars[index]     & ~0xFu;
            const u32 size_hi = bars[index + 1];

            bars[index]     = orig_lo;
            bars[index + 1] = orig_hi;

            const u64 size_mask = (static_cast<u64>(size_hi) << 32) | size_lo;
            info.size = ~size_mask + 1u;

        } else {
            info.is_64_bit       = false;
            info.is_prefetchable = (raw >> 3) & 1u;
            info.address         = raw & ~0xFULL;

            const u32 orig = bars[index];
            bars[index] = 0xFFFFFFFFu;
            const u32 mask = bars[index] & ~0xFu;
            bars[index] = orig;
            info.size = ~mask + 1u;
        }

        return info;
    }

} // namespace pci::bar
