// pci_config_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 23.05.26.
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
#ifndef VESPERAOS_PCI_CONFIG_REGS_H
#define VESPERAOS_PCI_CONFIG_REGS_H

/**
 * @brief GMCH Graphics Control register (GGC_0_0_0_PCI).
 *
 * @note Located at PCI config space offset 0x50 on Device 0:0:0 (Host Bridge).
 * @note All bits are LT-lockable. Once bit 0 (lock) is set by BIOS, the
 *       entire register becomes read-only. Never write to this register —
 *       read it once at init and derive sizes from it.
 * @note GMS field encoding is non-linear: 0x00–0x10 use 32 MB steps,
 *       0x20/0x30/0x40 are sparse 512 MB steps, 0xF0–0xFE use 4 MB steps.
 *       Use @ref GGC::dsm_size_bytes() to resolve the actual value.
 */
union GGC_0_0_0_PCI {
    struct {
        u16 lock        : 1;   ///< [0]    GGC Lock — set by BIOS, makes all bits RO  (R/W Key Lock)
        u16 ivd         : 1;   ///< [1]    IGD VGA Disable (0=VGA enabled, 1=disabled) (R/W Lock)
        u16 vamen       : 1;   ///< [2]    Versatile Acceleration Mode Enable           (R/W Lock)
        u16 reserved3_5 : 3;   ///< [5:3]  MBZ
        u16 ggms        : 2;   ///< [7:6]  GTT Graphics Memory Size                    (R/W Lock)
                               ///<          0x0 = No prealloc
                               ///<          0x1 = 2 MB GSM
                               ///<          0x2 = 4 MB GSM
                               ///<          0x3 = 8 MB GSM
        u16 gms         : 8;   ///< [15:8] Graphics Mode Select — DSM size             (R/W Lock)
                               ///<          0x00 = 0 MB
                               ///<          0x01–0x10 = N×32 MB  (32–512 MB)
                               ///<          0x20 = 1024 MB
                               ///<          0x30 = 1536 MB
                               ///<          0x40 = 2048 MB
                               ///<          0xF0–0xFE = (N−0xF0+1)×4 MB  (4–60 MB)
                               ///<          0xFF = Reserved
    } __attribute__((packed));
    u16 raw;

    /**
     * @brief Resolves the GMS field to the actual DSM (Data Stolen Memory) size in bytes.
     * @return DSM size in bytes, or 0 if GMS is reserved/unset.
     */
    [[nodiscard]] u64 dsm_size_bytes() const {
        if (gms == 0x00) return 0;
        if (gms <= 0x10) return static_cast<u64>(gms) * 32ull * 1024 * 1024;
        if (gms == 0x20) return 1024ull * 1024 * 1024;
        if (gms == 0x30) return 1536ull * 1024 * 1024;
        if (gms == 0x40) return 2048ull * 1024 * 1024;
        if (gms >= 0xF0 && gms <= 0xFE) return static_cast<u64>(gms - 0xF0 + 1) * 4ull * 1024 * 1024;
        return 0;  // Reserved
    }

    /**
     * @brief Resolves the GGMS field to the actual GSM (GTT Stolen Memory) size in bytes.
     * @return GSM size in bytes (0, 2MB, 4MB, or 8MB).
     */
    [[nodiscard]] u64 gsm_size_bytes() const {
        switch (ggms) {
            case 0x1: return 2ull * 1024 * 1024;
            case 0x2: return 4ull * 1024 * 1024;
            case 0x3: return 8ull * 1024 * 1024;
            default:  return 0;
        }
    }

    /**
     * @brief Derives the number of valid GGTT entries from the GSM size.
     *
     * Each GTT entry is 8 bytes and maps one 4 KB page.
     * GSM holds the actual GTT entry array, so entry count = GSM / 8.
     *
     * @return Number of valid GGTT entries, or 0 if GSM is not preallocated.
     */
    [[nodiscard]] u32 ggtt_entry_count() const {
        const u64 gsm = gsm_size_bytes();
        if (gsm == 0) return 0;
        return static_cast<u32>(gsm / 8);  // 8 bytes per PTE
    }
};

static_assert(sizeof(GGC_0_0_0_PCI) == 2, "GGC_0_0_0_PCI must be 16 bits");

constexpr u32 GGTT_FIRMWARE_RESERVED_ENTRIES = 0x1000;

constexpr u8 GGC_PCI_OFFSET = 0x50;


/**
 * @brief GMADR – Graphics Memory Range Address (PCI BAR2/BAR3, offset 0x18).
 *
 * 64-bit prefetchable memory BAR describing the CPU-visible aperture window.
 * The aperture size is NOT encoded here directly — bits [31:27] are either
 * part of the base address or address mask depending on MSAC.APSZ.
 * Use @ref MSAC_0_2_0_PCI to determine the actual aperture size.
 *
 * @note Memory/IO (bit 0), Memory Type (bits [2:1]=0b10), and Prefetchable
 *       (bit 3) are all hardwired RO. Never write to this register — the OS
 *       base address is set by the firmware.
 */
union GMADR_0_2_0_PCI {
    struct {
        // DWord 0 (BAR2)
        u32 mem_io_space    : 1;   ///< [0]     Hardwired 0 = memory space        (RO)
        u32 mem_type        : 2;   ///< [2:1]   Hardwired 0b10 = 64-bit BAR       (RO)
        u32 prefetchable    : 1;   ///< [3]     Hardwired 1 = prefetchable         (RO)
        u32 addr_mask_lo    : 23;  ///< [26:4]  Hardwired 0 (≥128MB guaranteed)   (RO)
        u32 apsz_256mb      : 1;   ///< [27]    256MB mask / base[27] per MSAC    (R/W Lock)
        u32 apsz_512mb      : 1;   ///< [28]    512MB mask / base[28] per MSAC    (R/W Lock)
        u32 apsz_1024mb     : 1;   ///< [29]    1024MB mask / base[29] per MSAC   (R/W Lock)
        u32 apsz_2048mb     : 1;   ///< [30]    2048MB mask / base[30] per MSAC   (R/W Lock)
        u32 apsz_4096mb     : 1;   ///< [31]    4096MB mask / base[31] per MSAC   (R/W Lock)

        // DWord 1 (BAR3)
        u32 base_hi         : 7;   ///< [38:32] Base address bits [38:32]          (R/W)
        u32 reserved        : 25;  ///< [63:39] MBZ (>512GB not supported)         (RO)
    } __attribute__((packed));

    struct {
        u32 lo;   ///< Raw BAR2
        u32 hi;   ///< Raw BAR3
    } dwords;

    u64 raw;

    /**
     * @brief Extracts the 64-bit physical base address of the aperture.
     * Masks off the flag bits [3:0] from the low dword.
     */
    [[nodiscard]] u64 base_address() const {
        return (static_cast<u64>(dwords.hi) << 32) | (dwords.lo & 0xFFFFFFF0u);
    }
};

static_assert(sizeof(GMADR_0_2_0_PCI) == 8, "GMADR must be 64 bits");

/**
 * @brief MSAC – Multi Size Aperture Control (PCI offset 0x62, Device 0:2:0).
 *
 * Determines the size of the graphics memory aperture (GMADR).
 * Bits [4:0] encode the aperture size; bits [7:5] are scratch R/W.
 *
 * Valid APSZ encodings (bits [4:0]):
 *   0b00000 (0x00) →  128 MB  — GMADR[26:4]  hardwired 0
 *   0b00001 (0x01) →  256 MB  — GMADR[27:4]  overridden 0  (default)
 *   0b00011 (0x03) →  512 MB  — GMADR[28:27] overridden 0
 *   0b00111 (0x07) → 1024 MB  — GMADR[29:27] overridden 0
 *   0b01111 (0x0F) → 2048 MB  — GMADR[30:27] overridden 0
 *   0b11111 (0x1F) → 4096 MB  — GMADR[31:27] overridden 0
 *
 * Illegal values are treated by hardware as the next valid value:
 *   0x02        → treated as 0x03  (512 MB)
 *   0x04–0x06   → treated as 0x07  (1024 MB)
 *   0x08–0x0E   → treated as 0x0F  (2048 MB)
 *   0x10–0x1E   → treated as 0x1F  (4096 MB)
 *
 * @note TXT-locked: becomes fully read-only once trusted environment launches.
 * @note Default value 0x01 → 256 MB aperture on most Gen9 platforms.
 */
union MSAC_0_2_0_PCI {
    struct {
        u8 apsz     : 5;  ///< [4:0] Untrusted Aperture Size bits [4:0]  (R/W Key)
        u8 scratch  : 3;  ///< [7:5] Scratch bits                         (R/W)
    } __attribute__((packed));
    u8 raw;

    /**
     * @brief Resolves bits [4:0] to the actual aperture size in bytes.
     *
     * Mirrors hardware's illegal-value promotion: any value between two valid
     * encodings is promoted to the next valid one, matching hardware behaviour.
     */
    [[nodiscard]] u64 aperture_size_bytes() const {
        switch (apsz) {
            case 0x00: return  128ull * 1024 * 1024;
            case 0x01: return  256ull * 1024 * 1024;

            case 0x02:          // illegal → treated as 0x03
            case 0x03: return  512ull * 1024 * 1024;

            case 0x04:          // illegal → treated as 0x07
            case 0x05:
            case 0x06:
            case 0x07: return 1024ull * 1024 * 1024;

            case 0x08:          // illegal → treated as 0x0F
            case 0x09:
            case 0x0A:
            case 0x0B:
            case 0x0C:
            case 0x0D:
            case 0x0E:
            case 0x0F: return 2048ull * 1024 * 1024;

            default:            // 0x10–0x1F, all → treated as 0x1F
                return 4096ull * 1024 * 1024;
        }
    }
};

static_assert(sizeof(MSAC_0_2_0_PCI) == 1, "MSAC must be 8 bits");

constexpr u8 MSAC_PCI_OFFSET = 0x62;

/**
 * @brief Decoded GMADR (Graphics Memory Address Range) descriptor.
 *
 * BAR2 of the Intel IGP (Device 0:2:0) is a 64-bit prefetchable memory BAR
 * that describes the CPU-visible aperture window into GPU address space.
 * The GPU's GTT translates accesses within this window to physical pages.
 */
struct GmadrInfo {
    u64 base;          ///< Physical base address of the aperture (from BAR2/BAR3).
    u64 size;          ///< Aperture window size in bytes (from BAR sizing trick).
    bool valid;        ///< False if BAR2 is absent, I/O-type, or sizing failed.
};

/**
 * @brief PCI Configuration Space layout for Intel Gen9/9.5 iGPU (Device 0:2:0).
 *
 * Covers the full 256-byte config space as documented in:
 * "Intel 8th/9th Generation Core Processor Families Datasheet, Volume 2"
 * Table 4-1: "Summary of Bus 0, Device 2, Function 0 (CFG)"
 *
 * @note Never write to this config space at runtime except via the Command
 *       register (0x04). All other R/W fields are configured by firmware.
 */
struct INTEL_IGP_PCI_CONFIG {

    // Standard PCI header fields (0x00–0x0F)
    u16 vendor_id;              ///< [0x00] VID2   — hardwired 0x8086
    u16 device_id;              ///< [0x02] DID2   — e.g. 0x5917 (UHD 620)
    u16 command;                ///< [0x04] PCICMD — Bus Master, Mem Space, INTx
    u16 status;                 ///< [0x06] PCISTS2
    u8  revision_id;            ///< [0x08] RID2
    u8  prog_if;                ///< [0x09] CC[7:0]
    u8  subclass;               ///< [0x0A] CC[15:8]
    u8  class_code;             ///< [0x0B] CC[23:16]
    u8  cache_line_size;        ///< [0x0C] CLS
    u8  latency_timer;          ///< [0x0D] MLT2
    u8  header_type;            ///< [0x0E] HDR2
    u8  bist;                   ///< [0x0F] (not listed, hardwired 0)

    // BARs (0x10–0x27) — named for what they actually contain

    // [0x10–0x17] GTTMMADR — 16 MB: 2 MB MMIO + 6 MB reserved + 8 MB GTT
    u32 gttmmadr_lo;            ///< [0x10] GTTMMADR low  32 bits (type=64bit, prefetchable=0)
    u32 gttmmadr_hi;            ///< [0x14] GTTMMADR high 32 bits

    // [0x18–0x1F] GMADR — CPU-visible aperture window into GPU address space
    u32 gmadr_lo;               ///< [0x18] GMADR low  32 bits (type=64bit, prefetchable=1)
    u32 gmadr_hi;               ///< [0x1C] GMADR high 32 bits

    // [0x20–0x23] IOBAR — legacy I/O BAR (typically unused by OS drivers)
    u32 iobar;                  ///< [0x20] I/O Base Address (bit 0 = I/O space indicator)

    // [0x24–0x27] hardwired 0
    u32 bar5_reserved;          ///< [0x24] Hardwired 0

    // Remaining standard header fields (0x28–0x3F)
    u32 cardbus_cis_ptr;        ///< [0x28] Hardwired 0
    u16 subsystem_vendor_id;    ///< [0x2C] SVID2
    u16 subsystem_id;           ///< [0x2E] SID2
    u32 expansion_rom_base;     ///< [0x30] ROMADR — Video BIOS ROM base
    u8  capabilities_ptr;       ///< [0x34] CAPPOINT — hardwired 0x40
    u8  pad_35[3];              ///< [0x35–0x37] Reserved
    u32 pad_38;                 ///< [0x38] Reserved
    u8  interrupt_line;         ///< [0x3C] INTRLINE
    u8  interrupt_pin;          ///< [0x3D] INTRPIN  — hardwired 0x01
    u8  min_grant;              ///< [0x3E] MINGNT
    u8  max_latency;            ///< [0x3F] MAXLAT

    // Device-specific region (0x40–0xFF)

    u8  pad_40[4];              ///< [0x40–0x43] Reserved

    u32 capid0_a;               ///< [0x44] Capabilities A
    u32 capid0_b;               ///< [0x48] Capabilities B

    u8  pad_4c[8];              ///< [0x4C–0x53] Reserved

    u32 deven0;                 ///< [0x54] Device Enable

    u8  pad_58[4];              ///< [0x58–0x5B] Reserved

    u32 bdsm;                   ///< [0x5C] Base of Data Stolen Memory

    u8  pad_60[2];              ///< [0x60–0x61] Reserved

    MSAC_0_2_0_PCI msac;        ///< [0x62] Multi Size Aperture Control

    u8  pad_63[13];             ///< [0x63–0x6F] Reserved

    u16 pciecaphdr;             ///< [0x70] PCIe Capability Header

    u8  pad_72[58];             ///< [0x72–0xAB] PCIe capability body (out of scope)

    u16 msi_capid;              ///< [0xAC] MSI Capability ID
    u16 msi_mc;                 ///< [0xAE] MSI Message Control
    u32 msi_ma;                 ///< [0xB0] MSI Message Address
    u16 msi_md;                 ///< [0xB4] MSI Message Data

    u8  pad_b6[26];             ///< [0xB6–0xCF] Reserved

    u16 pm_capid;               ///< [0xD0] Power Management Capability ID
    u16 pm_cap;                 ///< [0xD2] Power Management Capabilities
    u16 pm_cs;                  ///< [0xD4] Power Management Control/Status

    u8  pad_d6[42];             ///< [0xD6–0xFF] Reserved
} __attribute__((packed));

static_assert(offsetof(INTEL_IGP_PCI_CONFIG, capid0_a)  == 0x44, "CAPID0_A at 0x44");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, capid0_b)  == 0x48, "CAPID0_B at 0x48");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, deven0)    == 0x54, "DEVEN0 at 0x54");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, bdsm)      == 0x5C, "BDSM at 0x5C");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, msac)      == 0x62, "MSAC at 0x62");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, pciecaphdr) == 0x70, "PCIECAPHDR at 0x70");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, msi_capid) == 0xAC, "MSI at 0xAC");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, msi_ma)    == 0xB0, "MSI_MA at 0xB0");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, pm_capid)  == 0xD0, "PMCAPID at 0xD0");
static_assert(offsetof(INTEL_IGP_PCI_CONFIG, pm_cs)     == 0xD4, "PMCS at 0xD4");
static_assert(sizeof(INTEL_IGP_PCI_CONFIG)              == 0x100, "IGP config must be 256 bytes");

#endif  // VESPERAOS_PCI_CONFIG_REGS_H
