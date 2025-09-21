// xhci_device.h
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

#ifndef VESPERAOS_XHCI_DEVICE_H
#define VESPERAOS_XHCI_DEVICE_H

#include <cstdint>
#include "xhci_rings.h"
#include "xhci_device_ctx.h"
#include "xhci_usb_interface.h"
#include <dev/usb_xhci_ioctl.h>

class xhciDevice {
public:
    explicit xhciDevice(uint8_t slot_id, uint8_t port_num, uint8_t speed, bool use_64bit_context);

    void allocate_control_ep_ring();

    inline uint8_t get_slot_id() const { return info.slot_id; }
    inline uint8_t get_port_id() const { return info.port_num; }
    inline uint8_t get_speed() const { return info.speed; }
    inline uintptr_t get_input_context_phys() const { return m_input_context_phys; }
    inline xhciTransferRing *get_control_transfer_ring() const { return m_control_transfer_ring; }

    xhci_input_control_context32 *get_input_control_ctx();

    xhci_slot_context32 *get_input_slot_ctx();

    xhci_endpoint_context32 *get_input_control_ep_ctx();

    xhci_endpoint_context32 *get_input_ep_ctx(uint8_t endpoint_num);

    void setup_add_interface(const usb_interface_descriptor *desc);

    void sync_input_ctx(void *out_ctx);

    Vector<xhciUsbInterface *> interfaces;

    xhci_device_stat info;

private:
    bool use64byte_ctx;

    void *m_input_context;
    uintptr_t m_input_context_phys;
    xhciTransferRing *m_control_transfer_ring;



    void allocate_contexts();

    void allocate_input_context();

    void prepare_input_context();
};

#endif //VESPERAOS_XHCI_DEVICE_H
