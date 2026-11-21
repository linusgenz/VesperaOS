// intel_forcewake.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
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
#ifndef VESPERAOS_INTEL_FORCEWAKE_H
#define VESPERAOS_INTEL_FORCEWAKE_H

#include <vespera/types.h>

namespace gpu::intel::core {
    /// ACK bit shared by every Gen9 ForceWake domain's ack register
    inline constexpr u32 FORCEWAKE_ACK_BIT = 0x1;

    /**
     * @brief Descriptor for one Gen9 ForceWake power domain.
     *
     * Gen9 splits ForceWake per engine — Render, Blitter, and Media each have
     * their own request+ack MMIO register pair. IntelGpuDevice itself has no
     * opinion on which domains exist or what their offsets are — it just
     * drives whichever descriptor the caller hands it.
     */
    struct ForceWakeDomain {
        u32 request_reg;  ///< MMIO offset of the request register
        u32 ack_reg;      ///< MMIO offset of the ack register
        u32 enable_value; ///< value to write to the request register
        u32 ack_bit;      ///< bit to poll for in the ack register
        u32 timeout_us;
    };

    inline constexpr u32 FORCEWAKE_RENDER = 0xA278;
    inline constexpr u32 FORCEWAKE_ACK_RENDER = 0x0D84;
    inline constexpr u32 FORCEWAKE_RENDER_ENABLE = 0x00010001;
    inline constexpr u32 FORCEWAKE_RENDER_TIMEOUT = 1000;

    inline constexpr ForceWakeDomain RENDER_FORCEWAKE_DOMAIN{
        .request_reg = FORCEWAKE_RENDER,
        .ack_reg = FORCEWAKE_ACK_RENDER,
        .enable_value = FORCEWAKE_RENDER_ENABLE,
        .ack_bit = FORCEWAKE_ACK_BIT,
        .timeout_us = FORCEWAKE_RENDER_TIMEOUT,
    };

    inline constexpr u32 FORCEWAKE_BLITTER = 0xA188;
    inline constexpr u32 FORCEWAKE_ACK_BLITTER = 0x130044;
    inline constexpr u32 FORCEWAKE_BLITTER_ENABLE = 0x00010001;
    inline constexpr u32 FORCEWAKE_BLITTER_TIMEOUT = 1000;

    inline constexpr ForceWakeDomain BLITTER_FORCEWAKE_DOMAIN{
        .request_reg = FORCEWAKE_BLITTER,
        .ack_reg = FORCEWAKE_ACK_BLITTER,
        .enable_value = FORCEWAKE_BLITTER_ENABLE,
        .ack_bit = FORCEWAKE_ACK_BIT,
        .timeout_us = FORCEWAKE_BLITTER_TIMEOUT
    };
} // namespace gpu::intel::core

#endif  // VESPERAOS_INTEL_FORCEWAKE_H
