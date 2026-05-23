// xhci_dbc_ctx.h
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

#ifndef VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_CONTEXT_H
#define VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_CONTEXT_H

#include <vespera/types.h>

#include "../usb_descriptors.h"
#include "xhci_device_ctx.h"

// xHCI Debug Capability (DbC) context data structures.
// Reference: xHCI Specification, Section 7.6.9.
//
// The DbC Context is a contiguous 192-byte region pointed to by DCCP.
// It must be 16-byte aligned (DCCP bits 3:0 are RsvdP).
// In practice, allocate with 64-byte alignment to satisfy XHCI_DEVICE_CONTEXT_ALIGNMENT.
//
// Layout:
//   [0x00] DBC_INFO_CONTEXT      64 bytes   DbC Info Context  (DbCIC)
//   [0x40] DBC_ENDPOINT_CONTEXT  64 bytes   EP OUT context    (Debug Host → Target)
//   [0x80] DBC_ENDPOINT_CONTEXT  64 bytes   EP IN  context    (Target → Debug Host)

namespace usb {

// ============================================================================
//  String pointer mask for DbCIC address fields.
//  All four descriptor address fields have bit 0 == RsvdZ, so
//  the pointer must be 2-byte aligned.  Plain malloc/alloc_xhci_memory
//  satisfies this trivially; the mask is provided for documentation.
// ============================================================================

constexpr u64 DBC_IC_STR_PTR_MASK = ~static_cast<u64>(0x1);

// ============================================================================
//  Debug Capability Info Context (DbCIC)       Section 7.6.9.1
//
//  64-byte structure that defines the USB string descriptors presented
//  by the Debug Device during enumeration.
//
//  If a string is unused, set its Length field to 0;
//  the xHC will ignore the corresponding address field.
// ============================================================================

struct __attribute__((packed)) DBC_INFO_CONTEXT {
    // 0x00 — String 0 descriptor physical address (64-bit, bit 0 RsvdZ)
    // Points to a USB_STRING_DESCRIPTOR with the supported Language IDs.
    // Minimum: { length=4, type=0x03, lang_id=0x0409 }  (English US)
    u64 string0_ptr;

    // 0x08 — Manufacturer string descriptor physical address (64-bit, bit 0 RsvdZ)
    u64 manufacturer_ptr;

    // 0x10 — Product string descriptor physical address (64-bit, bit 0 RsvdZ)
    u64 product_ptr;

    // 0x18 — Serial number string descriptor physical address (64-bit, bit 0 RsvdZ)
    // Points to a USB_STRING_DESCRIPTOR with a UTF-16LE serial number string.
    // If unused: set serial_length = 0 and leave this field as 0.
    u64 serial_number_ptr;

    // 0x20 — Packed byte lengths of the four string descriptors above.
    // Each field is the total byte length of the corresponding USB_STRING_DESCRIPTOR,
    // i.e. sizeof(header) + strlen_utf16le_bytes — identical to bLength in the descriptor.
    // Set to 0 for any unused string (serial number if none desired).
    u8  string0_length;       // bits  7:0  of DWORD at 0x20
    u8  manufacturer_length;  // bits 15:8
    u8  product_length;       // bits 23:16
    u8  serial_length;        // bits 31:24

    // 0x24-0x3F — RsvdZ.  Initialize to 0.
    u8 rsvdz[28];
};
static_assert(sizeof(DBC_INFO_CONTEXT) == 64);

static_assert(offsetof(DBC_INFO_CONTEXT, string0_ptr)      == 0x00);
static_assert(offsetof(DBC_INFO_CONTEXT, manufacturer_ptr) == 0x08);
static_assert(offsetof(DBC_INFO_CONTEXT, product_ptr)      == 0x10);
static_assert(offsetof(DBC_INFO_CONTEXT, serial_number_ptr)== 0x18);
static_assert(offsetof(DBC_INFO_CONTEXT, string0_length)   == 0x20);


// Endpoint Type values for DBC_ENDPOINT_CONTEXT::ep_info2 bits [5:3].
constexpr u8 DBC_EP_TYPE_BULK_OUT = 2;   // host → target (OUT from host's perspective)
constexpr u8 DBC_EP_TYPE_BULK_IN  = 6;   // target → host (IN  from host's perspective)

// Max packet size for USB3 SuperSpeed bulk endpoints.
constexpr u16 DBC_MAX_PACKET_SIZE = 1024;

// Average TRB length for DbC bulk endpoints (bandwidth hint for xHC scheduler).
// 1024 is a safe default; tune if transfer sizes differ significantly.
constexpr u16 DBC_AVG_TRB_LENGTH  = 1024;

// ============================================================================
//  Complete DbC Context                               Section 7.6.9
//
//  Pointed to by DCCP.  Must be 16-byte aligned (DCCP bits 3:0 are RsvdP).
//
//  After allocation, zero the entire struct, then fill:
//    1. ctx.info   — string descriptor pointers + lengths
//    2. ctx.ep_out — bulk OUT endpoint context  (dbc_make_ep_info2 / dbc_make_tr_dequeue_ptr)
//    3. ctx.ep_in  — bulk IN  endpoint context
//  Then write phys addr to DCCP before setting DCCTRL.DCE = 1.
// ============================================================================

struct __attribute__((packed)) DBC_CONTEXT {
    DBC_INFO_CONTEXT     info;    // 0x00-0x3F  DbC Info Context
    XHCI_ENDPOINT_CONTEXT64 ep_out; // 0x40-0x7F  Bulk OUT: Debug Host → Target
    XHCI_ENDPOINT_CONTEXT64 ep_in;  // 0x80-0xBF  Bulk IN:  Target → Debug Host
};
static_assert(sizeof(DBC_CONTEXT) == 192);

static_assert(offsetof(DBC_CONTEXT, info)   == 0x00);
static_assert(offsetof(DBC_CONTEXT, ep_out) == 0x40);
static_assert(offsetof(DBC_CONTEXT, ep_in)  == 0x80);

// Language ID constant for String 0 descriptor.
constexpr u16 DBC_LANGID_EN_US = 0x0409;

} // namespace usb

#endif // VESPERAOS_DRIVERS_USB_XHCI_XHCI_DBC_CONTEXT_H