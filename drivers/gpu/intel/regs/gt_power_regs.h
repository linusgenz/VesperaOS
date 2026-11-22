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


constexpr usize GEN6_RPNSWREQ_OFFSET = 0xA008;

/**
 * @brief Render P-State Normal SW Request - Offset A008h in the GTTMMADR MMIO window.
 *
 * Used by software to request a target graphics frequency (ratio) for the
 * Integrated GFX Engine (GT). Bit layout varies across Intel generations.
 *
 * @note Located at physical address (GTTMMADR base) + 0xA008 (PCI 00:02.0 BAR0).
 * @note Requires Render Forcewake (e.g., FORCEWAKE_RENDER_GEN9 at 0xA278) to be
 *       active before writing on Gen 6+ hardware.
 */
union GEN6_RPNSWREQ {
    struct {
        u32 reserved0        : 15; ///< [14:0]  Reserved / power gating hints
        u32 aggressive_turbo : 4;  ///< [18:15] Turbo aggressiveness (0 = aggressive)
        u32 offset           : 6;  ///< [24:19] Frequency offset
        u32 frequency        : 6;  ///< [30:25] Requested frequency ratio (50 MHz units)
        u32 turbo_disable    : 1;  ///< [31]    1 = Disable turbo, 0 = Enable turbo
    } __attribute__((packed)) gen6;

    struct {
        u32 reserved0        : 15; ///< [14:0]  Reserved / power gating hints
        u32 aggressive_turbo : 4;  ///< [18:15] Turbo aggressiveness (0 = aggressive)
        u32 offset           : 5;  ///< [23:19] Frequency offset
        u32 frequency        : 7;  ///< [30:24] Requested frequency ratio (50 MHz units)
        u32 turbo_disable    : 1;  ///< [31]    1 = Disable turbo, 0 = Enable turbo
    } __attribute__((packed)) hsw;

    struct {
        u32 ignore_slice_ratio : 1;  ///< [0]     0 = Ignore separate slice ratio (use unslice)
        u32 reserved0          : 12; ///< [12:1]  Reserved
        u32 media_freq_ratio   : 1;  ///< [13]    Gen 12+ media frequency ratio select
        u32 reserved1          : 9;  ///< [22:14] Reserved
        u32 frequency          : 9;  ///< [31:23] Requested unslice frequency ratio (16.66 MHz units)
    } __attribute__((packed)) gen9;

    u32 raw;

    [[nodiscard]] static constexpr GEN6_RPNSWREQ make_gen6(
        const u32 target_mhz, const bool disable_turbo = false
    ) {
        GEN6_RPNSWREQ req{};
        req.gen6.turbo_disable = disable_turbo ? 1u : 0u;
        req.gen6.frequency = (target_mhz / 50u) & 0x3Fu;
        req.gen6.aggressive_turbo = 0u;
        return req;
    }

    [[nodiscard]] static constexpr GEN6_RPNSWREQ make_hsw(
        const u32 target_mhz, const bool disable_turbo = false
    ) {
        GEN6_RPNSWREQ req{};
        req.hsw.turbo_disable = disable_turbo ? 1u : 0u;
        req.hsw.frequency = (target_mhz / 50u) & 0x7Fu;
        req.hsw.aggressive_turbo = 0u;
        return req;
    }

    [[nodiscard]] static constexpr GEN6_RPNSWREQ make_gen9(const u32 target_mhz) {
        GEN6_RPNSWREQ req{};
        // Gen 9+ encodes frequency in units of 16.66 MHz (50 / 3 MHz), like in RPSTAT1
        req.gen9.frequency = ((target_mhz * 3u) / 50u) & 0x1FFu;
        req.gen9.ignore_slice_ratio = 0u;
        return req;
    }
};

#endif //VESPERAWORKSPACE_GT_POWER_REGS_H
