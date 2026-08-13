// fuse_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.08.26.
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

#ifndef VESPERAOS_FUSE_REGS_H
#define VESPERAOS_FUSE_REGS_H

#include <vespera/types.h>

constexpr u32 FUSE2_MMIO = 0x9120;

/**
 * @brief Mirror of FUSE2 Control DW register (FUSE2).
 *
 * @note Register Space: MMIO: 0/2/0
 * @note Source: BSpec
 * @note Default Value: 0x00000000
 * @note Size: 32 bits
 * @note Offset Address: 0x9120
 *
 * This register is read-only (RO) and contains hardware fuse configurations
 * for GT SKU, slice/subslice enables, media engines, and system capability fuses.
 */
union FUSE2 {
    struct {
        u32 capability_fuse : 16;  ///< [15:0]  Capability Fuse (Bit 2: 0b=Pre-Prod, 1b=Post-Prod quality FW) (RO)
        u32 spares3          : 2;   ///< [17:16] Reserved / Spares3 (RO)
        u32 vd_vebox_config : 2;   ///< [19:18] GT VDBox and VEBox Configuration Fuse (RO)
                                    ///<          00b = Both VDBOXes and VEBOXes enabled
                                    ///<          01b = VDBOX1 and VEBOX1 enabled (GT1,2)
                                    ///<          10b = VDBOX0 and VEBOX0 enabled (GT1,2)
        u32 subslice_disable : 4;  ///< [23:20] GT Subslice Disable Fuse (RO)
                                    ///<          Bit 20 - Subslice0 Disable
                                    ///<          Bit 21 - Subslice1 Disable
                                    ///<          Bit 22 - Subslice2 Disable
                                    ///<          Bit 23 - Subslice3 Disable
        u32 spares1          : 1;   ///< [24]    Reserved / Spares1 (RO)
        u32 slice_enable     : 3;   ///< [27:25] GT Slice Enable Fuse (RO)
                                    ///<          Bit 25 - Slice0 Enable
                                    ///<          Bit 26 - Slice1 Enable
                                    ///<          Bit 27 - Slice2 Enable
        u32 spares           : 1;   ///< [28]    Reserved / Spares (RO)
        u32 gt_sku_fuse      : 3;   ///< [31:29] GT SKU Fuse (RO)
    } __attribute__((packed));

    u32 raw;

    /**
     * @brief Checks if the firmware capability requires production quality FW.
     * @return true if post-production (only production FW allowed), false if pre-production.
     */
    [[nodiscard]] bool is_post_production() const {
        return (capability_fuse & (1u << 2)) != 0;
    }

    /**
     * @brief Helper to check if a specific GT Slice is enabled.
     * @param slice_idx Index of the slice (0 to 2)
     * @return true if enabled, false otherwise
     */
    [[nodiscard]] bool is_slice_enabled(u8 slice_idx) const {
        if (slice_idx > 2) return false;
        return (slice_enable & (1u << slice_idx)) != 0;
    }

    /**
     * @brief Helper to check if a specific GT Subslice is disabled.
     * @param subslice_idx Index of the subslice (0 to 3)
     * @return true if disabled, false otherwise
     */
    [[nodiscard]] bool is_subslice_disabled(u8 subslice_idx) const {
        if (subslice_idx > 3) return false;
        return (subslice_disable & (1u << subslice_idx)) != 0;
    }
};

static_assert(sizeof(FUSE2) == 4, "FUSE2 register must be 32 bits");

#endif  // VESPERAOS_FUSE_REGS_H