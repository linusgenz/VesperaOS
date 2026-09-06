// mocs_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 04.09.26.
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

#ifndef VESPERAOS_MOCS_REGS_H
#define VESPERAOS_MOCS_REGS_H

#include <vespera/types.h>

namespace gpu::intel::core {

    constexpr u32 GFX_MOCS_BASE = 0xC800;
    constexpr usize GFX_MOCS_COUNT = 64;
    constexpr u32 GFX_MOCS_STRIDE = 4;

    union GFX_MOCS {
        struct {
            u32 llc_edram_cacheability : 2;  ///< [1:0]   00=PTE/UC-with-fence, 01=UC, 10=WT, 11=WB
            u32 target_cache           : 2;  ///< [3:2]   00=eLLC only, 01=LLC only, 10/11=LLC/eLLC allowed
            u32 lru_management         : 2;  ///< [5:4]   11=likely hits, 10=poor hits, 01=don't touch on hit
            u32 dont_allocate_on_miss  : 1;  ///< [6]     1 = don't allocate line on cache miss (RO surfaces)
            u32 enable_skip_caching    : 1;  ///< [7]     1 = enable LLC skip-cache mechanism
            u32 skip_caching_control   : 3;  ///< [10:8]  skip-cache address-bit control, only if bit7=1
            u32 page_faulting_mode     : 3;  ///< [13:11] 000 = use global mode from context descriptor
            u32 reserved14             : 1;  ///< [14]    Reserved
            u32 reserved15_31          : 17; ///< [31:15] Reserved
        } __attribute__((packed));

        u32 raw;

        enum Cacheability : u32 {
            CACHE_PTE_OR_UC_WITH_FENCE = 0b00,
            CACHE_UNCACHEABLE          = 0b01,
            CACHE_WRITETHROUGH         = 0b10,
            CACHE_WRITEBACK            = 0b11,
        };

        enum TargetCache : u32 {
            TARGET_ELLC_ONLY      = 0b00,
            TARGET_LLC_ONLY       = 0b01,
            TARGET_LLC_ELLC       = 0b10, ///< 0b11 is equivalent per PRM
        };

        enum Lru : u32 {
            LRU_RESERVED       = 0b00,
            LRU_DONT_CHANGE    = 0b01,
            LRU_POOR_HIT_AGE   = 0b10,
            LRU_GOOD_HIT_AGE   = 0b11,
        };

        /// Fully-uncached entry: bypasses LLC/eDRAM caching outright. This is the encoding
        /// GFX_MOCS_1 must hold for every PTE built with MOCS_UNCACHED (ppgtt.h) to behave as its
        /// name promises, instead of falling through to whatever the hardware reset default is.
        [[nodiscard]] static constexpr GFX_MOCS uncached() {
            GFX_MOCS m{};
            m.llc_edram_cacheability = CACHE_UNCACHEABLE;
            m.target_cache = TARGET_LLC_ELLC;
            m.lru_management = LRU_DONT_CHANGE;
            return m;
        }

        /// Cached, writeback entry for GFX_MOCS_9 (index 9 = MOCS_CACHED_WB in ppgtt.h).
        [[nodiscard]] static constexpr GFX_MOCS cached_writeback() {
            GFX_MOCS m{};
            m.llc_edram_cacheability = CACHE_WRITEBACK;
            m.target_cache = TARGET_LLC_ELLC;
            m.lru_management = LRU_GOOD_HIT_AGE;
            return m;
        }
    };

    static_assert(sizeof(GFX_MOCS) == 4);

}  // namespace gpu::intel::core

#endif  // VESPERAOS_MOCS_REGS_H
