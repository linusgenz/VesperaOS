// mocs_init.cpp
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

#include "mocs_init.h"

#include <vespera/log.h>

#include "intel_gpu_device.h"
#include "mocs_regs.h"
#include "ggtt_allocator.h"

namespace gpu::intel::core {
    namespace {
        constexpr u32 kGfxMocsBase = 0xC800;
        constexpr u32 kLncfcMocsBase = 0xB020; // GEN9_LNCFCMOCS(0), bitte verifizieren
        constexpr u32 kEntries = 64;
        constexpr u32 kL3Regs = kEntries / 2;

        struct Entry {
            u32 control;
            u16 l3cc;
        };

        constexpr Entry kPte{0x30, 0x30};
        constexpr Entry kUncached{0x09, 0x10};
        constexpr Entry kWbLlcEllc{0x3B, 0x30};
        constexpr Entry kWbLlc{0x37, 0x30};

        constexpr Entry entry_for(const u32 idx) {
            switch (idx) {
                case 0:  return kUncached;        // I915_MOCS_UNCACHED / Mesa uncached
                case 1:  return kPte;             // I915_MOCS_PTE (MUSS 0x38 sein, NICHT 0x30!)
                case 2:  return kWbLlcEllc;       // I915_MOCS_CACHED / Mesa WB
                case 3:  return {0x05, 0x10};     // UC LLC
                case 4:  return {0x07, 0x30};     // WB LLC, age 0
                case 5:  return {0x37, 0x10};     // WB LLC, L3 UC
                case 62: return {0x37, 0x10};
                case 63: return {0x37, 0x10};
                default: return kPte;             // 0x38 (LLC+eLLC via PTE)
            }
        }
    }

    void MocsTable::init() const {
        auto wr = [&](u32 off, u32 v) { *reinterpret_cast<volatile u32*>(device_.mmio_base() + off) = v; };
        auto rd = [&](u32 off) { return *reinterpret_cast<volatile u32*>(device_.mmio_base() + off); };

        u32 bad = 0;

        for (u32 i = 0; i < kEntries; i++) {
            const u32 want = entry_for(i).control;
            wr(kGfxMocsBase + i * 4, want);
            if (const u32 got = rd(kGfxMocsBase + i * 4); got != want) {
                if (bad++ < 4)
                    Log::error("intel-mocs: GFX_MOCS[%u] readback 0x%08x != 0x%08x", i, got, want);
            }
        }

        for (u32 i = 0; i < kL3Regs; i++) {
            const u32 want = static_cast<u32>(entry_for(2 * i).l3cc) |
                (static_cast<u32>(entry_for(2 * i + 1).l3cc) << 16);
            wr(kLncfcMocsBase + i * 4, want);
            if (const u32 got = rd(kLncfcMocsBase + i * 4); got != want) {
                if (bad++ < 4)
                    Log::error("intel-mocs: LNCFCMOCS[%u] readback 0x%08x != 0x%08x", i, got, want);
            }
        }

        Log::info("intel-mocs: GFX_MOCS + LNCFCMOCS programmed, %u mismatches", bad);
    }
} // namespace gpu::intel::core
