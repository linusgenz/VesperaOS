// xhci_device.cpp
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

#include "xhci.h"
#include "xhci_trb.h"
#include <cstdint>
#include "xhci_device.h"
#include <log.h>
#include "xhci_device_ctx.h"
#include "xhci_usb_interface.h"

xhciDevice::xhciDevice(uint8_t slot_id, uint8_t port_num, uint8_t speed, bool use_64byte_ctx)
    : use64byte_ctx(use_64byte_ctx) {
    info.port_num = port_num;
    info.slot_id = slot_id;
    info.speed = speed;
    allocate_input_context();
    allocate_control_ep_ring();
}

void xhciDevice::allocate_control_ep_ring() {
    m_control_transfer_ring = xhciTransferRing::allocate(info.slot_id);
}

void xhciDevice::allocate_input_context() {
    uint64_t input_context_size = use64byte_ctx ? sizeof(xhci_input_context64) : sizeof(xhci_input_context32);
    m_input_context = alloc_xhci_memory(
        input_context_size,
        XHCI_INPUT_CONTROL_CONTEXT_ALIGNMENT,
        XHCI_INPUT_CONTROL_CONTEXT_BOUNDARY
    );
    memset(m_input_context, 0, input_context_size);

    m_input_context_phys = xhci_get_physical_addr(m_input_context);
}

xhci_input_control_context32* xhciDevice::get_input_control_ctx() {
    if (use64byte_ctx) {
        auto* input_ctx = static_cast<xhci_input_context64*>(m_input_context);
        return reinterpret_cast<xhci_input_control_context32*>(&input_ctx->control_context);
    } else {
        auto* input_ctx = static_cast<xhci_input_context32*>(m_input_context);
        return &input_ctx->control_context;
    }
}

xhci_slot_context32* xhciDevice::get_input_slot_ctx() {
    if (use64byte_ctx) {
        auto* input_ctx = static_cast<xhci_input_context64*>(m_input_context);
        return reinterpret_cast<xhci_slot_context32*>(&input_ctx->device_context.slot_context);
    } else {
        auto* input_ctx = static_cast<xhci_input_context32*>(m_input_context);
        return &input_ctx->device_context.slot_context;
    }
}

xhci_endpoint_context32* xhciDevice::get_input_control_ep_ctx() {
    if (use64byte_ctx) {
        auto* input_ctx = static_cast<xhci_input_context64*>(m_input_context);
        return reinterpret_cast<xhci_endpoint_context32*>(&input_ctx->device_context.control_ep_context);
    } else {
        auto* input_ctx = static_cast<xhci_input_context32*>(m_input_context);
        return &input_ctx->device_context.control_ep_context;
    }
}

xhci_endpoint_context32* xhciDevice::get_input_ep_ctx(uint8_t endpoint_num) {
    uint8_t endpoint_index = endpoint_num - 2;

    if (use64byte_ctx) {
        xhci_input_context64* input_ctx = static_cast<xhci_input_context64*>(m_input_context);
        return reinterpret_cast<xhci_endpoint_context32*>(&input_ctx->device_context.ep[endpoint_index]);
    } else {
        xhci_input_context32* input_ctx = static_cast<xhci_input_context32*>(m_input_context);
        return &input_ctx->device_context.ep[endpoint_index];
    }
}

void xhciDevice::setup_add_interface(const usb_interface_descriptor* desc) {
    auto iface = new xhciUsbInterface(info.slot_id, desc);
    interfaces.push_back(iface);
}

void xhciDevice::sync_input_ctx(void* out_ctx) {
    if (use64byte_ctx) {
        auto* input_ctx = static_cast<xhci_input_context64*>(m_input_context);
        xhci_device_context64* input_device_ctx = &input_ctx->device_context;
        memcpy(input_device_ctx, out_ctx, sizeof(xhci_device_context64));
    } else {
        auto* input_ctx = static_cast<xhci_input_context32*>(m_input_context);
        xhci_device_context32* input_device_ctx = &input_ctx->device_context;
        memcpy(input_device_ctx, out_ctx, sizeof(xhci_device_context32));
    }
}