// mchbar_regs.h
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

#ifndef VESPERAOS_REGS_MCHBAR_REGS_H
#define VESPERAOS_REGS_MCHBAR_REGS_H

#include <vespera/types.h>

namespace gpu::intel::regs {
    /**
     * @brief RP state limits - Offset 5994h in the MCHBAR MMIO window.
     *
     * Lets software cap the maximum base frequency allowed for the
     * Integrated GFX Engine (GT) during run-time.
     *
     * @note Located at physical address (MCHBAR base) + 0x5994, NOT in
     *       PCI config space. MCHBAR base comes from the Host Bridge
     *       PCI register at offset 0x48 (see @ref pci::MCHBAR_BASE_0_0_0_PCI).
     * @note Default value FFh (no limit imposed).
     */
    union RP_STATE_LIMITS_MCHBAR {
        struct {
            u32 rpstt_lim : 8;  ///< [7:0]   Max GT base frequency limit (RW)
            u32 reserved  : 24; ///< [31:8]  Reserved (RO)
        } __attribute__((packed));

        u32 raw;

        [[nodiscard]] constexpr bool has_limit() const noexcept {
            return rpstt_lim != 0xFFu;
        }

        [[nodiscard]] constexpr u32 limit_freq_mhz() const noexcept {
            return rpstt_lim * 50u;
        }
    };

    static_assert(sizeof(RP_STATE_LIMITS_MCHBAR) == 4, "RP_STATE_LIMITS_MCHBAR must be 32 bits");

    inline constexpr u32 RP_STATE_LIMITS_MCHBAR_OFFSET = 0x5994;

    /**
     * @brief RP state capabilities — Offset 5998h in the MCHBAR MMIO window.
     *
     * Reports the maximum base frequency capability for the Integrated
     * GFX Engine (GT): the highest (RP0), the guaranteed (RP1), and the
     * lowest/minimum (RPN) operating points. All fields are read-only,
     * in units of 50 MHz.
     *
     * @note Located at physical address (MCHBAR base) + 0x5998.
     * @note RP0_CAP = maximum (turbo) frequency.
     * @note RP1_CAP = maximum guaranteed (non-turbo) frequency.
     * @note RPN_CAP = minimum frequency.
     */
    union RP_STATE_CAP_MCHBAR {
        struct {
            u32 rp0_cap  : 8; ///< [7:0]   Max base frequency capability, 50 MHz units (ROS)
            u32 rp1_cap  : 8; ///< [15:8]  Max guaranteed base frequency, 50 MHz units  (ROS)
            u32 rpn_cap  : 8; ///< [23:16] Min base frequency capability, 50 MHz units  (ROS)
            u32 reserved : 8; ///< [31:24] Reserved (RO)
        } __attribute__((packed));

        u32 raw;

        /// @brief Maximum (turbo) GT frequency in MHz.
        [[nodiscard]] u32 max_freq_mhz() const {
            return rp0_cap * 50u;
        }

        /// @brief Maximum guaranteed (non-turbo) GT frequency in MHz.
        [[nodiscard]] u32 guaranteed_freq_mhz() const {
            return rp1_cap * 50u;
        }

        /// @brief Minimum GT frequency in MHz.
        [[nodiscard]] u32 min_freq_mhz() const {
            return rpn_cap * 50u;
        }
    };

    static_assert(sizeof(RP_STATE_CAP_MCHBAR) == 4, "RP_STATE_CAP_MCHBAR must be 32 bits");

    inline constexpr u32 RP_STATE_CAP_MCHBAR_OFFSET = 0x5998;
} // namespace gpu::intel::regs

#endif // VESPERAOS_REGS_MCHBAR_REGS_H
