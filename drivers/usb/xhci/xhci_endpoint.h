// xhci_endpoint.h
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

#ifndef VESPERAOS_XHCI_ENDPOINT_H
#define VESPERAOS_XHCI_ENDPOINT_H

#include "xhci_rings.h"
#include "../usb_descriptors.h"

class XhciEndpoint {
public:
    XhciEndpoint(u8 xhc_slot_id, const USB_ENDPOINT_DESCRIPTOR* desc);
    ~XhciEndpoint() = default;

    u8     usb_endpoint_addr;
    u8     usb_endpoint_attributes;
    u16    max_packet_size;
    u8     interval;
    u8     xhc_endpoint_type;
    u8     xhc_endpoint_num;

    [[nodiscard]] u8* get_data_buffer() const { return data_buffer_; }
    [[nodiscard]] uptr get_data_buffer_dma() const { return data_buffer_dma_addr_; }

    [[nodiscard]] XhciTransferRing* get_transfer_ring() const
    {
        return transfer_ring_;
    }

private:
    u8*    data_buffer_;
    uptr   data_buffer_dma_addr_;
    XhciTransferRing* transfer_ring_;

    void allocate_internal_data_buffer();
};

#endif //VESPERAOS_XHCI_ENDPOINT_H