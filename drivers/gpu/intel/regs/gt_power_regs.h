// gt_power_regs.h
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

#ifndef VESPERAWORKSPACE_GT_POWER_REGS_H
#define VESPERAWORKSPACE_GT_POWER_REGS_H

#include <vespera/types.h>

inline constexpr usize GEN6_RPSTAT1_OFFSET = 0xA01C;

/**
 * @brief Render P-State Status 1 - Offset A01Ch in the GTTMMADR MMIO window.
 *
 * Reports the current actual graphics frequency (CAGF) of the Integrated
 * GFX Engine (GT). Bit layout varies across Intel graphics generations.
 *
 * @note Located at physical address (GTTMMADR base) + 0xA01C (PCI 00:02.0 BAR0),
 *       NOT in MCHBAR or PCI config space.
 */
union GEN6_RPSTAT1 {
    struct {
        u32 reserved0 : 8;  ///< [7:0]   Reserved / other status bits
        u32 cagf      : 7;  ///< [14:8]  Current Actual Graphics Frequency (Gen 6 / Gen 7 IVB)
        u32 reserved1 : 17; ///< [31:15] Reserved / other status bits
    } __attribute__((packed)) gen6;

    struct {
        u32 reserved0 : 7;  ///< [6:0]   Reserved / other status bits
        u32 cagf      : 7;  ///< [13:7]  Current Actual Graphics Frequency (Haswell / Broadwell)
        u32 reserved1 : 18; ///< [31:14] Reserved / other status bits
    } __attribute__((packed)) hsw;

    struct {
        u32 reserved0 : 23; ///< [22:0]  Reserved / other status bits
        u32 cagf      : 9;  ///< [31:23] Current Actual Graphics Frequency (Gen 9+ Skylake & newer)
    } __attribute__((packed)) gen9;

    u32 raw;

    [[nodiscard]] constexpr u32 gen6_freq_mhz() const noexcept {
        return gen6.cagf * 50u;
    }

    [[nodiscard]] constexpr u32 hsw_freq_mhz() const noexcept {
        return hsw.cagf * 50u;
    }

    [[nodiscard]] constexpr u32 gen9_freq_mhz() const noexcept {
        // Gen 9+ encodes CAGF in units of 16.66 MHz (50 / 3 MHz)
        return (gen9.cagf * 50u) / 3u;
    }
};

#endif //VESPERAWORKSPACE_GT_POWER_REGS_H
