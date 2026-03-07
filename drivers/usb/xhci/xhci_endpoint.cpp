// xhci_endpoint.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 25.08.25.
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

#include "xhci_endpoint.h"
#include "../usb_descriptors.h"

u8 get_xhc_endpoint_type_from_ep_descriptor(const USB_ENDPOINT_DESCRIPTOR* desc) {
    u8 endpoint_direction_in = (desc->b_endpoint_address & 0x80) ? 1 : 0;

    // transfer type
    switch (desc->bm_attributes & 0x3) {
        case 0: {
            return XHCI_ENDPOINT_TYPE_CONTROL;
        }
        case 1: {
            return endpoint_direction_in ? XHCI_ENDPOINT_TYPE_ISOCHRONOUS_IN : XHCI_ENDPOINT_TYPE_ISOCHRONOUS_OUT;
        }
        case 2: {
            return endpoint_direction_in ? XHCI_ENDPOINT_TYPE_BULK_IN : XHCI_ENDPOINT_TYPE_BULK_OUT;
        }
        case 3: {
            return endpoint_direction_in ? XHCI_ENDPOINT_TYPE_INTERRUPT_IN : XHCI_ENDPOINT_TYPE_INTERRUPT_OUT;
        }
        default: break;
    }

    return 0;
}

u8 get_xhc_endpoint_num_from_ep_descriptor(const USB_ENDPOINT_DESCRIPTOR* desc) {
    u8 endpoint_number_base = desc->b_endpoint_address & 0x0F;
    u8 endpoint_direction_in = (desc->b_endpoint_address & 0x80) ? 1 : 0;

    return (endpoint_number_base * 2) + endpoint_direction_in;
}

XhciEndpoint::XhciEndpoint(u8 xhc_slot_id, const USB_ENDPOINT_DESCRIPTOR* desc) {
    usb_endpoint_addr = desc->b_endpoint_address;
    usb_endpoint_attributes = desc->bm_attributes;
    max_packet_size = desc->w_max_packet_size;
    interval = desc->b_interval;

    xhc_endpoint_type = get_xhc_endpoint_type_from_ep_descriptor(desc);
    xhc_endpoint_num = get_xhc_endpoint_num_from_ep_descriptor(desc);

    transfer_ring_ = XhciTransferRing::allocate(xhc_slot_id);

    allocate_internal_data_buffer();
}

static usize next_power_of_two(usize x) {
    if (x <= 1) return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#if UINTPTR_MAX > 0xffffffff
    x |= x >> 32; // für 64bit
#endif
    return x + 1;
}


void XhciEndpoint::allocate_internal_data_buffer() {
    usize alignment = next_power_of_two(max_packet_size);
    usize boundary  = alignment;

    if (alignment < 64) {
        alignment = 64;
        boundary  = 64;
    }

    data_buffer_ = static_cast<u8*>(
        alloc_xhci_memory(max_packet_size, alignment, boundary));

    data_buffer_dma_addr_ = xhci_get_physical_addr(data_buffer_);
}