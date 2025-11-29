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

uint8_t get_xhc_endpoint_type_from_ep_descriptor(const usb_endpoint_descriptor* desc) {
    uint8_t endpoint_direction_in = (desc->bEndpointAddress & 0x80) ? 1 : 0;
    uint8_t transfer_type = desc->bmAttributes & 0x3;

    switch (transfer_type) {
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

uint8_t get_xhc_endpoint_num_from_ep_descriptor(const usb_endpoint_descriptor* desc) {
    uint8_t endpoint_number_base = desc->bEndpointAddress & 0x0F;
    uint8_t endpoint_direction_in = (desc->bEndpointAddress & 0x80) ? 1 : 0;

    return (endpoint_number_base * 2) + endpoint_direction_in;
}

xhciEndpoint::xhciEndpoint(uint8_t xhc_slot_id, const usb_endpoint_descriptor* desc) {
    usb_endpoint_addr = desc->bEndpointAddress;
    usb_endpoint_attributes = desc->bmAttributes;
    max_packet_size = desc->wMaxPacketSize;
    interval = desc->bInterval;

    xhc_endpoint_type = get_xhc_endpoint_type_from_ep_descriptor(desc);
    xhc_endpoint_num = get_xhc_endpoint_num_from_ep_descriptor(desc);

    m_transfer_ring = xhciTransferRing::allocate(xhc_slot_id);

    allocate_internal_data_buffer();
}

static size_t next_power_of_two(size_t x) {
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


void xhciEndpoint::allocate_internal_data_buffer() {
    size_t alignment = next_power_of_two(max_packet_size);
    size_t boundary  = alignment;

    if (alignment < 64) {
        alignment = 64;
        boundary  = 64;
    }

    m_data_buffer = reinterpret_cast<uint8_t*>(
        alloc_xhci_memory(max_packet_size, alignment, boundary));

    m_data_buffer_dma_addr = xhci_get_physical_addr(m_data_buffer);
}