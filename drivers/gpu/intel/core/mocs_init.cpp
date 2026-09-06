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

    void MocsTable::init() const {
        constexpr GFX_MOCS safe_default = GFX_MOCS::uncached();

        for (usize i = 0; i < GFX_MOCS_COUNT; i++) {
            GFX_MOCS entry = safe_default;

            switch (i) {
                case MOCS_UNCACHED:
                    entry = GFX_MOCS::uncached();
                    break;
                case MOCS_CACHED_WB:
                    entry = GFX_MOCS::cached_writeback();
                    break;
                default:
                    break;
            }

            const u32 offset = GFX_MOCS_BASE + static_cast<u32>(i) * GFX_MOCS_STRIDE;
            *reinterpret_cast<volatile u32*>(device_.mmio_base() + offset) = entry.raw;
        }

        Log::info("intel-mocs: programmed %u GFX_MOCS registers (base=0x%x)",
                  static_cast<u32>(GFX_MOCS_COUNT), GFX_MOCS_BASE);
    }

}  // namespace gpu::intel::core
