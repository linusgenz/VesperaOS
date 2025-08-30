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

#include <cstdint>
#include "xhci_rings.h"
#include "../usb_descriptors.h"

class xhciEndpoint {
public:
    xhciEndpoint(uint8_t xhc_slot_id, const usb_endpoint_descriptor* desc);
    ~xhciEndpoint() = default;

    uint8_t     usb_endpoint_addr;
    uint8_t     usb_endpoint_attributes;
    uint16_t    max_packet_size;
    uint8_t     interval;
    uint8_t     xhc_endpoint_type;
    uint8_t     xhc_endpoint_num;

    inline uint8_t* get_data_buffer() { return m_data_buffer; }
    inline uintptr_t get_data_buffer_dma() { return m_data_buffer_dma_addr; }

    inline xhciTransferRing* get_transfer_ring() {
        return m_transfer_ring;
    }

private:
    uint8_t*    m_data_buffer;
    uintptr_t   m_data_buffer_dma_addr;
    xhciTransferRing* m_transfer_ring;

    void allocate_internal_data_buffer();
};

#endif //VESPERAOS_XHCI_ENDPOINT_H