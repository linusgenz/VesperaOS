// xhci_hid_driver.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.09.25.
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

#include "xhci_hid_driver.h"
#include "xhci.h"

void XhciHidDriver::on_startup(usb::XhciDriver* hcd, XhciDevice* dev) {
    this->on_device_init(hcd);

    request_hid_report(hcd, dev);
}

void XhciHidDriver::on_event(usb::XhciDriver* hcd, XhciDevice* dev) {
    const auto& endpoint = interface_->endpoints[0];
    u8* data = endpoint->get_data_buffer();

    this->on_device_event(data);

    request_hid_report(hcd, dev);
}

void XhciHidDriver::request_hid_report(const usb::XhciDriver* hcd, const XhciDevice* dev) const
{
    const auto endpoint = interface_->endpoints[0];
    const auto transfer_ring = endpoint->get_transfer_ring();

    xhci_normal_trb_t normal_trb{};
    normal_trb.trb_type = XHCI_TRB_TYPE_NORMAL;
    normal_trb.data_buffer_physical_base = endpoint->get_data_buffer_dma();
    normal_trb.trb_transfer_length = endpoint->max_packet_size;
    normal_trb.ioc = 1;

    transfer_ring->enqueue(reinterpret_cast<xhci_trb_t*>(&normal_trb));

    hcd->ring_doorbell(dev->get_slot_id(), endpoint->xhc_endpoint_num);
}
