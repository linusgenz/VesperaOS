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

#include "xhci_rings.h"
#include "xhci_device_ctx.h"
#include "xhci_usb_interface.h"
#include <uapi/vespera/dev/usb_xhci_ioctl.h>

class XhciDevice {
public:
    explicit XhciDevice(uint8_t slot_id, uint8_t port_num, uint8_t speed, bool use_64_byte_ctx);

    void allocate_control_ep_ring();

    [[nodiscard]] uint8_t get_slot_id() const { return info.slot_id; }
    [[nodiscard]] uint8_t get_port_id() const { return info.port_num; }
    [[nodiscard]] uint8_t get_speed() const { return info.speed; }
    [[nodiscard]] uintptr_t get_input_context_phys() const { return input_context_phys_; }
    [[nodiscard]] XhciTransferRing *get_control_transfer_ring() const { return control_transfer_ring_; }

    [[nodiscard]] XHCI_INPUT_CONTROL_CONTEXT32 *get_input_control_ctx() const;

    [[nodiscard]] XHCI_SLOT_CONTEXT32 *get_input_slot_ctx() const;

    [[nodiscard]] XHCI_ENDPOINT_CONTEXT32 *get_input_control_ep_ctx() const;

    [[nodiscard]] XHCI_ENDPOINT_CONTEXT32 *get_input_ep_ctx(uint8_t endpoint_num) const;

    void setup_add_interface(const USB_INTERFACE_DESCRIPTOR *desc);

    void sync_input_ctx(const void *out_ctx) const;

    Vector<XhciUsbInterface *> interfaces;

    xhci_device_stat info{};

private:
    bool use_64_byte_ctx_;

    void *input_context_{};
    uintptr_t input_context_phys_{};
    XhciTransferRing *control_transfer_ring_{};

    void allocate_input_context();
};

#endif //VESPERAOS_XHCI_DEVICE_H
