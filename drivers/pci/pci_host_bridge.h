// pci_host_bridge.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 24.05.26.
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
#ifndef VESPERAOS_PCI_HOST_BRIDGE_H
#define VESPERAOS_PCI_HOST_BRIDGE_H

#include <vespera/types.h>

#include "pci.h"

// ReSharper disable CppInconsistentNaming

namespace pci {

    /**
     * @brief Common layout for Host Bridge base-address registers with
     *        1 MB granularity: BDSM, BGSM, TSEGMB, TOLUD.
     *
     * Bits [31:20] hold the physical address (bits 19:0 are implicitly 0).
     * Bit  [0]     is the KL-lock — once set, all RW_L fields become RO.
     * Bits [19:1]  are reserved RO, always read as 0.
     */
    union HB_ADDR1M_REG {
        struct {
            u32 lock : 1;   ///< [0]     Lock (RW_KL) — locks this register + all RW_L fields
            u32 rsvd : 19;  ///< [19:1]  Reserved (RO)
            u32 base : 12;  ///< [31:20] Address bits [31:20], 1 MB granularity (RW_L)
        } __attribute__((packed));

        u32 raw;

        /// Physical address with bits [19:0] forced to 0, ready for pointer arithmetic.
        [[nodiscard]] u32 address() const volatile {
            return raw & 0xFFF00000u;
        }

        /// True after firmware has locked the register (Intel TXT or explicit lock).
        [[nodiscard]] bool is_locked() const volatile {
            return lock != 0;
        }
    };

    using TOLUD_0_0_0_PCI = HB_ADDR1M_REG;   ///< [0xBC] Top of Low Usable DRAM
    using BDSM_0_0_0_PCI = HB_ADDR1M_REG;    ///< [0xB0] Base of Data Stolen Memory
    using BGSM_0_0_0_PCI = HB_ADDR1M_REG;    ///< [0xB4] Base of GTT Stolen Memory
    using TSEGMB_0_0_0_PCI = HB_ADDR1M_REG;  ///< [0xB8] TSEG Memory Base

    /**
     * @brief PCI Configuration Space layout for the Intel Host Bridge / DRAM Controller.
     *        Bus: 0, Device: 0, Function: 0.
     *
     * Covers the full 256-byte config space as documented in:
     * "Intel 8th/9th Generation Core Processor Families Datasheet, Volume 2"
     * Table 3-1: "Summary of Bus 0, Device 0, Function 0 (CFG)"
     *
     * @note The Host Bridge has no BARs — all six BAR fields in the Type-0 header
     *       are hardwired to 0. Do not attempt to size or map them.
     * @note All register bits are LT-lockable. Once firmware sets the lock, all
     *       R/W fields become RO. Read registers once at init and cache results.
     * @note Gaps between documented offsets are marked as reserved pads.
     *       Never write to reserved fields.
     */
    struct INTEL_HB_PCI_CONFIG {
        // Standard PCI Type-0 Header (0x00–0x3F)
        // VID=0x8086, DID=0x3EXX, BARs all 0 (hardwired)
        PCI_HEADER0 header;  ///< [0x00–0x3F]

        // Device-specific region (0x40–0xFF)

        // [0x40–0x47]
        u64 pxpepbar;  ///< [0x40] PCIe Egress Port Base Address

        // [0x48–0x4F]
        u64 mchbar;  ///< [0x48] Host MMIO Register Range Base

        // [0x50–0x51]
        u16 ggc;  ///< [0x50] GMCH Graphics Control (GGC)
                  ///<        DSM size [15:8], GSM size [7:6],
                  ///<        VAMEN [2], IVD [1], lock [0]

        // [0x52–0x53] Reserved
        u16 pad_52;

        // [0x54–0x57]
        u32 deven;  ///< [0x54] Device Enable
                    ///<        Controls which devices are visible on PCI

        // [0x58–0x5B]
        u32 pavpc;  ///< [0x58] Protected Audio Video Path Control

        // [0x5C–0x5F]
        u32 dpr;  ///< [0x5C] DMA Protected Range

        // [0x60–0x67]
        u64 pciexbar;  ///< [0x60] PCIe ECAM Base Address (MCFG base)

        // [0x68–0x6F]
        u64 dmibar;  ///< [0x68] DMI Register Range Base Address

        // [0x70–0x77]
        u64 meseg_base;  ///< [0x70] ME Base Address Register

        // [0x78–0x7F]
        u64 meseg_limit;  ///< [0x78] ME Limit Address Register

        // [0x80–0x86]
        u8 pam[7];  ///< [0x80] Programmable Attribute Maps 0–6
                    ///<        Controls ROM/RAM shadowing in legacy ranges

        // [0x87]
        u8 lac;  ///< [0x87] Legacy Access Control

        // [0x88]
        u8 smramc;  ///< [0x88] System Management RAM Control

        // [0x89–0x8F] Reserved
        u8 pad_89[7];

        // [0x90–0x97]
        u64 remapbase;  ///< [0x90] Remap Base Address Register

        // [0x98–0x9F]
        u64 remaplimit;  ///< [0x98] Remap Limit Address Register

        // [0xA0–0xA7]
        u64 tom;  ///< [0xA0] Top of Memory

        // [0xA8–0xAF]
        u64 touud;  ///< [0xA8] Top of Upper Usable DRAM
                    ///<        Physical RAM ceiling above 4GB

        // [0xB0–0xB3]
        BDSM_0_0_0_PCI bdsm;  ///< [0xB0] Base of Data Stolen Memory (DSM)

        // [0xB4–0xB7]
        BGSM_0_0_0_PCI bgsm;  ///< [0xB4] Base of GTT Stolen Memory (GSM)

        // [0xB8–0xBB]
        TSEGMB_0_0_0_PCI tsegmb;  ///< [0xB8] TSEG Memory Base

        // [0xBC–0xBF]
        TOLUD_0_0_0_PCI tolud;  ///< [0xBC] Top of Low Usable DRAM

        // [0xC0–0xC7] Reserved
        u8 pad_c0[8];

        // [0xC8–0xC9]
        u16 errsts;  ///< [0xC8] Error Status

        // [0xCA–0xCB]
        u16 errcmd;  ///< [0xCA] Error Command

        // [0xCC–0xCD]
        u16 smicmd;  ///< [0xCC] SMI Command

        // [0xCE–0xCF]
        u16 scicmd;  ///< [0xCE] SCI Command

        // [0xD0–0xDB] Reserved
        u8 pad_d0[12];

        // [0xDC–0xDF]
        u32 skpd;  ///< [0xDC] Scratchpad Data

        // [0xE0–0xE3] Reserved
        u8 pad_e0[4];

        // [0xE4–0xE7]
        u32 capid0_a;  ///< [0xE4] Capabilities A

        // [0xE8–0xEB]
        u32 capid0_b;  ///< [0xE8] Capabilities B

        // [0xEC–0xEF]
        u32 capid0_c;  ///< [0xEC] Capabilities C

        // [0xF0–0xFF] Reserved
        u8 pad_f0[16];

    } __attribute__((packed));

    static_assert(offsetof(INTEL_HB_PCI_CONFIG, pxpepbar) == 0x40, "PXPEPBAR at 0x40");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, mchbar) == 0x48, "MCHBAR at 0x48");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, ggc) == 0x50, "GGC at 0x50");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, deven) == 0x54, "DEVEN at 0x54");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, pavpc) == 0x58, "PAVPC at 0x58");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, dpr) == 0x5C, "DPR at 0x5C");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, pciexbar) == 0x60, "PCIEXBAR at 0x60");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, dmibar) == 0x68, "DMIBAR at 0x68");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, remapbase) == 0x90, "REMAPBASE at 0x90");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, touud) == 0xA8, "TOUUD at 0xA8");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, bdsm) == 0xB0, "BDSM at 0xB0");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, bgsm) == 0xB4, "BGSM at 0xB4");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, tolud) == 0xBC, "TOLUD at 0xBC");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, skpd) == 0xDC, "SKPD at 0xDC");
    static_assert(offsetof(INTEL_HB_PCI_CONFIG, capid0_a) == 0xE4, "CAPID0_A at 0xE4");
    static_assert(sizeof(INTEL_HB_PCI_CONFIG) == 0x100, "HB config space must be 256 bytes");

}  // namespace pci

#endif  // VESPERAOS_PCI_HOST_BRIDGE_H
