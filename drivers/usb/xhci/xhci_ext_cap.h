// xhci_ext_cap.h
//
// LuminOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 29.07.25.
//
// This file is part of LuminOS.
//
// LuminOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// LuminOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with LuminOS. If not, see <https://www.gnu.org/licenses/>.

#ifndef XHCI_EXT_CAP_H
#define XHCI_EXT_CAP_H
#include "stdint.h"

/*
// xHci Spec Section 7.2 (page 521)
At least one of these capability structures is required for all xHCI
implementations. More than one may be defined for implementations that
support more than one bus protocol. Refer to section 4.19.7 for more
information.
*/
struct xhci_usb_supported_protocol_capability {
    union {
        struct {
            uint8_t id;
            uint8_t next;
            uint8_t minor_revision_version;
            uint8_t major_revision_version;
        };

        // Extended capability entries must be read as 32-bit words
        uint32_t dword0;
    };

    union {
        uint32_t dword1;
        uint32_t name; // "USB "
    };

    union {
        struct {
            uint8_t compatible_port_offset;
            uint8_t compatible_port_count;
            uint8_t protocol_defined;
            uint8_t protocol_speed_id_count; // (PSIC)
        };

        uint32_t dword2;
    };

    union {
        struct {
            uint32_t slot_type: 4;
            uint32_t reserved: 28;
        };

        uint32_t dword3;
    };

    xhci_usb_supported_protocol_capability() = default;

    xhci_usb_supported_protocol_capability(volatile uint32_t *cap) {
        dword0 = cap[0];
        dword1 = cap[1];
        dword2 = cap[2];
        dword3 = cap[3];
    }
};

struct xhci_legacy_support_capability {
    union {
        struct {
            uint32_t capability_id: 8;
            uint32_t next_cap_ptr: 8;
            uint32_t bios_owned: 1; // Bit 16
            uint32_t reserved1: 7;
            uint32_t os_owned: 1; // Bit 24
            uint32_t reserved2: 7;
        } __attribute__((packed));

        uint32_t raw;
    } usblegsup;

    union {
        struct {
            uint32_t smi_on_eint: 1; // Bit 16
            uint32_t reserved0: 1; // Bit 17
            uint32_t reserved1: 1; // Bit 18
            uint32_t reserved2: 1; // Bit 19
            uint32_t smi_on_host_sys_error: 1; // Bit 20
            uint32_t reserved3: 8; // Bits 21–28
            uint32_t smi_on_os_ownership: 1; // Bit 29 (RW1C)
            uint32_t smi_on_pci_command: 1; // Bit 30 (RW1C)
            uint32_t smi_on_bar: 1; // Bit 31 (RW1C)
        } __attribute__((packed));

        uint32_t raw;
    } usblegctlsts;

    xhci_legacy_support_capability() = default;

    xhci_legacy_support_capability(volatile uint32_t *cap) {
        usblegsup.raw = cap[0];
        usblegctlsts.raw = cap[1];
    }
} __attribute__((packed));

#endif //XHCI_EXT_CAP_H
