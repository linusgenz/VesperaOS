// force_to_nonpriv_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 20.09.26.
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

#ifndef VESPERAOS_FORCE_TO_NONPRIV_REGS_H
#define VESPERAOS_FORCE_TO_NONPRIV_REGS_H

#include <vespera/types.h>

/// Engine-relative offset of slot 0. RCS: 0x2000 + 0x4D0 = 0x24D0 (Gen9 Vol 2c, FORCE_TO_NONPRIV).
    constexpr u32 ENGINE_FORCE_TO_NONPRIV_OFF = 0x4D0;

/// Gen9 exposes 12 slots per engine (FORCE_TO_NONPRIV_0..11).
    constexpr u32 FORCE_TO_NONPRIV_SLOT_COUNT = 12;

/// Hardware default of every slot (address field 0x825 -> MMIO offset 0x2094).
/// Unused slots are reprogrammed to this so no stale/random offset stays whitelisted.
    constexpr u32 FORCE_TO_NONPRIV_DEFAULT = 0x00002094;

/**
 * @brief FORCE_TO_NONPRIV: one whitelist slot (32 bit, R/W).
 *
 * Each slot names one MMIO register that the render command streamer
 * treats as non-privileged while processing register writes from a
 * non-privileged batch buffer. It extends the fixed non-privilege table
 * from the MI_BATCH_BUFFER_START description. Registers outside both
 * tables raise EIR.priv_violation. The registers are global and
 * power-context save/restored (Gen9 Vol 2c, FORCE_TO_NONPRIV).
 *
 * Layout: [31:26] MBZ | [25:2] MmioAddress[25:2] | [1:0] MBZ
 * Reset default: 0x825 in the address field (= MMIO offset 0x2094).
 */
union FORCE_TO_NONPRIV {
    u32 raw;

    struct {
        u32 reserved0       : 2;  ///< [1:0]   MBZ
        u32 mmio_address_dw : 24; ///< [25:2]  MmioAddress[25:2] (DWord index of the register)
        u32 reserved1       : 6;  ///< [31:26] MBZ
    };

    static constexpr u32 MMIO_ADDRESS_MASK = 0x03FFFFFCu; ///< MmioAddress[25:2] in place

        constexpr void set_register_offset(u32 mmio_offset) {
        raw = mmio_offset & MMIO_ADDRESS_MASK;
    }

    [[nodiscard]] constexpr u32 register_offset() const {
        return raw & MMIO_ADDRESS_MASK;
    }

    /// @return false if the offset cannot be represented (unaligned or above bit 25).
    [[nodiscard]] static constexpr bool is_whitelistable(u32 mmio_offset) {
        return (mmio_offset & ~MMIO_ADDRESS_MASK) == 0;
    }

    [[nodiscard]] static constexpr FORCE_TO_NONPRIV for_register(u32 mmio_offset) {
        FORCE_TO_NONPRIV v{};
        v.set_register_offset(mmio_offset);
        return v;
    }

    /// Reset-default slot (points at 0x2094), so an unused slot never keeps a stale register.
    [[nodiscard]] static constexpr FORCE_TO_NONPRIV unused() {
        return for_register(FORCE_TO_NONPRIV_DEFAULT);
    }
};

static_assert(sizeof(FORCE_TO_NONPRIV) == sizeof(u32), "FORCE_TO_NONPRIV must be exactly 4 bytes");

    constexpr u32 GEN8_L3SQCREG4 = 0xb118;
    constexpr u32 GEN9_CTX_PREEMPT_REG = 0x2248;
    constexpr u32 GEN8_CS_CHICKEN1 = 0x2580;
    constexpr u32 GEN8_HDC_CHICKEN1 = 0x7304;
    constexpr u32 COMMON_SLICE_CHICKEN2 = 0x7014;


#endif // VESPERAOS_FORCE_TO_NONPRIV_REGS_H
