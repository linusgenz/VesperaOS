// xhci_dbc_regs.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.05.26.
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

#ifndef VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_REGS_H
#define VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_REGS_H

#include <vespera/types.h>

// xHCI Debug Capability (DbC) MMIO register definitions.
// Reference: xHCI Specification, Section 7.6.8.

namespace usb {

    // ============================================================================
    //  Extended capability ID for the DbC block.
    //  Add this to XHCI_EXTENDED_CAPABILITY_CODE if not already present.
    // ============================================================================

    constexpr u8 XHCI_EXTCAP_ID_DBC = 0x0A;

    // ============================================================================
    //  Offset 0x00 — Debug Capability ID Register (DCID)
    //
    //  Bits  7:0   cap_id    Capability ID — always 0x0A for DbC (RO)
    //  Bits 15:8   next_ptr  Next xHCI Extended Capability Pointer (RO)
    //  Bits 20:16  erst_max  DCERST Max — max ERST entries = 2^erst_max (RO)
    //  Bits 31:21  RsvdP
    // ============================================================================

    union DBC_DCID_REGISTER {
        struct {
            u32 cap_id : 8;
            u32 next_ptr : 8;
            u32 erst_max : 5;
            u32 rsvdp : 11;
        };
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCID_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x04 — Debug Capability Doorbell Register (DCDB)
    //
    //  The DCDB register is disabled while DCCTRL.DRC == 1.
    //  Software must clear DRC before attempting to ring the doorbell.
    // ============================================================================

    union __attribute__((packed)) DBC_DCDB_REGISTER {
        struct __attribute__((packed)) {
            u32 rsvdp0 : 8;     ///< Bits  7:0  — RsvdP
            u32 db_target : 8;  ///< Bits 15:8  — Doorbell Target: 0 = EP1 OUT, 1 = EP1 IN
            u32 rsvdp1 : 8;     ///< Bits 23:16 — RsvdP
            u32 rsvdp2 : 8;     ///< Bits 31:24 — RsvdP (not in table but rounds to 32 bits)
        };
        u32 raw;
    };
    static_assert(sizeof(DBC_DCDB_REGISTER) == 4);

    constexpr u32 DBC_DB_TARGET_OUT = 0u;  ///< Data EP 1 OUT — host → target
    constexpr u32 DBC_DB_TARGET_IN = 1u;   ///< Data EP 1 IN  — target → host

    // ============================================================================
    //  Offset 0x08 — Debug Capability ERST Size Register (DCERSTSZ)
    //
    //  Bits 15:0   erst_size  Number of valid ERST entries (max = 2^DCID.erst_max)
    //  Bits 31:16  RsvdP
    //
    //  Must be initialized before setting DCCTRL.DCE = 1.
    // ============================================================================

    union DBC_DCERSTSZ_REGISTER {
        struct {
            u32 erst_size : 16;
            u32 rsvdp : 16;
        };
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCERSTSZ_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x10 — Debug Capability ERST Base Address Register (DCERSTBA)
    //  Offset 0x18 — Debug Capability Event Ring Dequeue Pointer (DCERDP)
    //  Offset 0x30 — Debug Capability Context Pointer (DCCP)
    //
    //  These are raw u64 fields in DBC_REGS.  The masks below describe
    //  which bits carry the address and which are reserved or overloaded.
    //
    //  DCERSTBA  bits  3:0  RsvdP    — address must be 16-byte aligned
    //            bits 63:4  address
    //
    //  DCERDP    bits  2:0  DESI     — Dequeue ERST Segment Index
    //            bit   3    RsvdP
    //            bits 63:4  pointer  — must be 16-byte aligned
    //
    //  DCCP      bits  3:0  RsvdP    — pointer must be 16-byte aligned
    //            bits 63:4  pointer
    //
    //  All three must be initialized before setting DCCTRL.DCE = 1.
    // ============================================================================

    constexpr u64 DBC_DCERSTBA_ADDR_MASK = ~static_cast<u64>(0xF);

    constexpr u64 DBC_DCCP_ADDR_MASK = ~static_cast<u64>(0xF);

    union DBC_DCERDP_REGISTER {
        struct {
            u64 desi : 3;      // 2:0 Dequeue ERST Segment Index (DESI) - RW
            u64 rsvdp0 : 1;    // 3 Reserved, Preserved
            u64 deq_ptr : 60;  // 63:4 Dequeue Pointer (high bits of 64-bit address)
        } __attribute__((packed));

        u64 raw;
    } __attribute__((packed));

    static_assert(sizeof(DBC_DCERDP_REGISTER) == 8);

    // ============================================================================
    //  Offset 0x20 — Debug Capability Control Register (DCCTRL)
    //
    //  Bit  0      dcr        DbC Run (RO)
    //                         '1' = Debug Device is in Configured state;
    //                              bulk data pipe transactions are accepted.
    //                         Cleared by any port state transition that exits
    //                         the Configured state (e.g. port reset).
    //  Bit  1      lse        Link Status Event Enable (RW)
    //                         '1' = generate Port Status Change Events on PLC.
    //  Bit  2      hot        Halt OUT TR (RW1S)
    //                         While '1': STALL TPs issued for all IN TPs on the
    //                         OUT TR.  Cleared by ClearFeature(ENDPOINT_HALT).
    //                         Only valid when dcr == 1.
    //  Bit  3      hit        Halt IN TR (RW1S)
    //                         While '1': STALL TPs issued for all OUT DPs on the
    //                         IN TR.  Cleared by ClearFeature(ENDPOINT_HALT).
    //                         Only valid when dcr == 1.
    //  Bit  4      drc        DbC Run Change (RW1C)
    //                         Set when dcr transitions 1 → 0.
    //                         While '1' the DCDB doorbell is disabled.
    //                         Software must clear this bit to re-enable DCDB.
    //  Bits 15:5   RsvdP
    //  Bits 23:16  max_burst  Debug Max Burst Size (RO) — vendor defined
    //  Bits 30:24  dev_addr   USB Device Address assigned during enumeration (RO)
    //                         Valid only when dcr == 1.
    //  Bit  31     dce        Debug Capability Enable (RW)
    //                         Set to '1' to enable DbC operation.
    //                         Clearing releases the Root Hub port and terminates
    //                         all Transfer/Event Ring activity.
    // ============================================================================

    union DBC_DCCTRL_REGISTER {
        struct {
            u32 dcr : 1;
            u32 lse : 1;
            u32 hot : 1;
            u32 hit : 1;
            u32 drc : 1;
            u32 rsvdp : 11;
            u32 max_burst : 8;
            u32 dev_addr : 7;
            u32 dce : 1;
        } __attribute__((packed));
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCCTRL_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x24 — Debug Capability Status Register (DCST)
    //
    //  Bit  0      er        Event Ring Not Empty (RO)
    //                        '1' = DbC Event Ring contains at least one event.
    //                        Auto-cleared when enqueue ptr == DCERDP.
    //  Bit  1      sbr       DbC System Bus Reset (RO)
    //                        '1' = chip-level reset or PCI RST# resets the DbC.
    //                        '0' = only HCRST/LHCRST reset the DbC.
    //  Bits 23:2   RsvdP
    //  Bits 31:24  port_num  Root Hub port assigned to DbC (RO)
    //                        '0' = DbC is not attached to any port.
    // ============================================================================

    union DBC_DCST_REGISTER {
        struct {
            u32 er : 1;
            u32 sbr : 1;
            u32 rsvdp : 22;
            u32 port_num : 8;
        };
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCST_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x28 — Debug Capability Port Status and Control Register (DCPORTSC)
    //
    //  IMPORTANT: This register has device-side semantics.  Field definitions
    //  differ significantly from the host-side XHCI_PORTSC_REGISTER.
    //
    //  Bit  0      ccs        Current Connect Status (RO)
    //                         '1' = a Debug Host is connected and assigned to DbC.
    //                         '0' if DCE == 0.
    //  Bit  1      ped        Port Enabled/Disabled (RW)
    //                         '1' = port is enabled.
    //                         Set automatically on 0→1 of ccs or 1→0 of pr.
    //                         '0' if DCE or CCS are '0'.
    //  Bits 3:2    RsvdZ
    //  Bit  4      pr         Port Reset (RO)
    //                         '1' = bus reset sequence detected (in progress).
    //                         Cleared when reset completes; DbC → USB Default state.
    //                         A 0→1 transition clears ped.
    //  Bits 8:5    pls        Port Link State (RO) — see dbc_port_link_state
    //  Bit  9      RsvdZ
    //  Bits 13:10  port_speed Port Speed (RO)
    //                         PSI value per xHCI section 7.2.1.
    //                         DbC does NOT support LS/FS/HS — USB3 SuperSpeed only.
    //                         Undefined if CCS == 0.
    //  Bits 16:14  RsvdZ
    //  Bit  17     csc        Connect Status Change (RW1C)
    //                         Set on any change to ccs.  Clear by writing '1'.
    //  Bits 20:18  RsvdZ
    //  Bit  21     prc        Port Reset Change (RW1C)
    //                         Set when pr transitions 1→0 (reset complete).
    //  Bit  22     plc        Port Link Status Change (RW1C)
    //                         Set on: U0→U3, U3→U0, Polling→Disabled,
    //                                 Ux/Recovery→Inactive.
    //  Bit  23     cec        Port Config Error Change (RW1C)
    //                         Set when the port fails to configure its link partner.
    //  Bits 31:24  RsvdZ
    // ============================================================================

    union DBC_DCPORTSC_REGISTER {
        struct {
            u32 ccs : 1;
            u32 ped : 1;
            u32 rsvdz1 : 2;
            u32 pr : 1;
            u32 pls : 4;
            u32 rsvdz2 : 1;
            u32 port_speed : 4;
            u32 rsvdz3 : 3;
            u32 csc : 1;
            u32 rsvdz4 : 3;
            u32 prc : 1;
            u32 plc : 1;
            u32 cec : 1;
            u32 rsvdz5 : 8;
        };
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCPORTSC_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x38 — Debug Capability Device Descriptor Info Register 1 (DCDDI1)
    //
    //  Bits  7:0   dbc_protocol  bInterfaceProtocol in USB Interface Descriptor
    //                            0 = vendor defined
    //                            1 = GNU Remote Debug Command Set
    //  Bits 15:8   RsvdZ
    //  Bits 31:16  vendor_id     idVendor in USB Device Descriptor
    //
    //  Must be initialized before DCE = 1.
    // ============================================================================

    union DBC_DCDDI1_REGISTER {
        struct {
            u32 dbc_protocol : 8;
            u32 rsvdz : 8;
            u32 vendor_id : 16;
        } __attribute__((packed));
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCDDI1_REGISTER) == 4);

    // ============================================================================
    //  Offset 0x3C — Debug Capability Device Descriptor Info Register 2 (DCDDI2)
    //
    //  Bits 15:0   product_id      idProduct in USB Device Descriptor
    //  Bits 31:16  device_revision bcdDevice in USB Device Descriptor
    //
    //  Must be initialized before DCE = 1.
    // ============================================================================

    union DBC_DCDDI2_REGISTER {
        struct {
            u32 product_id : 16;
            u32 device_revision : 16;
        } __attribute__((packed));
        u32 raw;
    } __attribute__((packed));
    static_assert(sizeof(DBC_DCDDI2_REGISTER) == 4);

    // ============================================================================
    //  Port Link State values for DBC_DCPORTSC_REGISTER::pls
    // ============================================================================

    enum class dbc_port_link_state : u8 {
        U0 = 0,
        U1 = 1,
        U2 = 2,
        U3 = 3,  // suspended
        DISABLED = 4,
        RX_DETECT = 5,
        INACTIVE = 6,
        POLLING = 7,
        RECOVERY = 8,
        HOT_RESET = 9,
    };

    // ============================================================================
    //  DbC Protocol values for DBC_DCDDI1_REGISTER::dbc_protocol
    // ============================================================================

    enum class dbc_protocol : u8 {
        VENDOR_DEFINED = 0,
        GNU_REMOTE_DEBUG = 1,
    };

    // ============================================================================
    //  Complete DbC MMIO Register Block                          total = 0x40 bytes
    //
    //  Map over:  xhc_base + (ext_cap_offset_for_DbC_entry)
    //
    //  Layout (spec section 7.6.8):
    //    0x00  dcid      DCID     (32-bit)
    //    0x04  dcdb      DCDB     (32-bit)
    //    0x08  dcerstsz  DCERSTSZ (32-bit)
    //    0x0C  _pad0              (32-bit RsvdP — gap before 64-bit DCERSTBA)
    //    0x10  dcerstba  DCERSTBA (64-bit)
    //    0x18  dcerdp    DCERDP   (64-bit)
    //    0x20  dcctrl    DCCTRL   (32-bit)
    //    0x24  dcst      DCST     (32-bit)
    //    0x28  dcportsc  DCPORTSC (32-bit)
    //    0x2C  _pad1              (32-bit RsvdP — gap before 64-bit DCCP)
    //    0x30  dccp      DCCP     (64-bit)
    //    0x38  dcddi1    DCDDI1   (32-bit)
    //    0x3C  dcddi2    DCDDI2   (32-bit)
    // ============================================================================

    struct DBC_REGS {
        DBC_DCID_REGISTER dcid;          // 0x00
        DBC_DCDB_REGISTER dcdb;          // 0x04
        DBC_DCERSTSZ_REGISTER dcerstsz;  // 0x08
        u32 _pad0;                       // 0x0C  RsvdP
        u64 dcerstba;                    // 0x10  bits 3:0 RsvdP; use DBC_DCERSTBA_ADDR_MASK
        DBC_DCERDP_REGISTER dcerdp;      // 0x18  bits 2:0 DESI, bit 3 RsvdP; use DBC_DCERDP_*
        DBC_DCCTRL_REGISTER dcctrl;      // 0x20
        DBC_DCST_REGISTER dcst;          // 0x24
        DBC_DCPORTSC_REGISTER dcportsc;  // 0x28
        u32 _pad1;                       // 0x2C  RsvdP
        u64 dccp;                        // 0x30  bits 3:0 RsvdP; use DBC_DCCP_ADDR_MASK
        DBC_DCDDI1_REGISTER dcddi1;      // 0x38
        DBC_DCDDI2_REGISTER dcddi2;      // 0x3C
    } __attribute__((packed));
    static_assert(sizeof(DBC_REGS) == 0x40);

    static_assert(offsetof(DBC_REGS, dcid) == 0x00);
    static_assert(offsetof(DBC_REGS, dcdb) == 0x04);
    static_assert(offsetof(DBC_REGS, dcerstsz) == 0x08);
    static_assert(offsetof(DBC_REGS, dcerstba) == 0x10);
    static_assert(offsetof(DBC_REGS, dcerdp) == 0x18);
    static_assert(offsetof(DBC_REGS, dcctrl) == 0x20);
    static_assert(offsetof(DBC_REGS, dcst) == 0x24);
    static_assert(offsetof(DBC_REGS, dcportsc) == 0x28);
    static_assert(offsetof(DBC_REGS, dccp) == 0x30);
    static_assert(offsetof(DBC_REGS, dcddi1) == 0x38);
    static_assert(offsetof(DBC_REGS, dcddi2) == 0x3C);

}  // namespace usb

#endif  // VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_REGS_H