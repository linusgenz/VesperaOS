// xhci_ext_cap.h
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 29.07.25.
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

#ifndef XHCI_EXT_CAP_H
#define XHCI_EXT_CAP_H

/*
// xHci Spec Section 7.2 (page 521)
At least one of these capability structures is required for all xHCI
implementations. More than one may be defined for implementations that
support more than one bus protocol. Refer to section 4.19.7 for more
information.
*/
struct XhciUsbSupportedProtocolCapability {
    union {
        struct {
            u8 id;
            u8 next;
            u8 minor_revision_version;
            u8 major_revision_version;
        };

        // Extended capability entries must be read as 32-bit words
        u32 dword0;
    };

    union {
        u32 dword1;
        u32 name; // "USB "
    };

    union {
        struct {
            u8 compatible_port_offset;
            u8 compatible_port_count;
            u8 protocol_defined;
            u8 protocol_speed_id_count; // (PSIC)
        };

        u32 dword2;
    };

    union {
        struct {
            u32 slot_type: 4;
            u32 reserved: 28;
        };

        u32 dword3;
    };

    XhciUsbSupportedProtocolCapability() = default;

    explicit XhciUsbSupportedProtocolCapability(const volatile u32 *cap) {
        dword0 = cap[0];
        dword1 = cap[1];
        dword2 = cap[2];
        dword3 = cap[3];
    }
};

struct XHCI_LEGACY_SUPPORT_CAPABILITY {
    union {
        struct {
            u32 capability_id: 8;
            u32 next_cap_ptr: 8;
            u32 bios_owned: 1; // Bit 16
            u32 reserved1: 7;
            u32 os_owned: 1; // Bit 24
            u32 reserved2: 7;
        } __attribute__((packed));

        u32 raw;
    } usblegsup;

    union {
        struct {
            u32 smi_on_eint: 1; // Bit 16
            u32 reserved0: 1; // Bit 17
            u32 reserved1: 1; // Bit 18
            u32 reserved2: 1; // Bit 19
            u32 smi_on_host_sys_error: 1; // Bit 20
            u32 reserved3: 8; // Bits 21–28
            u32 smi_on_os_ownership: 1; // Bit 29 (RW1C)
            u32 smi_on_pci_command: 1; // Bit 30 (RW1C)
            u32 smi_on_bar: 1; // Bit 31 (RW1C)
        } __attribute__((packed));

        u32 raw;
    } usblegctlsts;

    XHCI_LEGACY_SUPPORT_CAPABILITY() = default;

    explicit XHCI_LEGACY_SUPPORT_CAPABILITY(const volatile u32 *cap) {
        usblegsup.raw = cap[0];
        usblegctlsts.raw = cap[1];
    }
} __attribute__((packed));

#endif //XHCI_EXT_CAP_H
