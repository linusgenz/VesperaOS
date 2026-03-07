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

#include "xhci_device.h"

#include "xhci.h"
#include "xhci_device_ctx.h"
#include "xhci_usb_interface.h"
#include <vespera/mm/memory.h>

XhciDevice::XhciDevice(const u8 slot_id, const u8 port_num, const u8 speed, const bool use_64_byte_ctx)
    : use_64_byte_ctx_(use_64_byte_ctx)
{
    info.port_num = port_num;
    info.slot_id = slot_id;
    info.speed = speed;
    allocate_input_context();
    allocate_control_ep_ring();
}

void XhciDevice::allocate_control_ep_ring()
{
    control_transfer_ring_ = XhciTransferRing::allocate(info.slot_id);
}

void XhciDevice::allocate_input_context()
{
    const u64 input_context_size = use_64_byte_ctx_ ? sizeof(XhciInputContext64) : sizeof(XhciInputContext32);
    input_context_ = alloc_xhci_memory(
        input_context_size,
        XHCI_INPUT_CONTROL_CONTEXT_ALIGNMENT,
        XHCI_INPUT_CONTROL_CONTEXT_BOUNDARY
    );
    memset(input_context_, 0, input_context_size);

    input_context_phys_ = xhci_get_physical_addr(input_context_);
}

XHCI_INPUT_CONTROL_CONTEXT32* XhciDevice::get_input_control_ctx() const
{
    if (use_64_byte_ctx_)
    {
        auto* input_ctx = static_cast<XhciInputContext64*>(input_context_);
        return reinterpret_cast<XHCI_INPUT_CONTROL_CONTEXT32*>(&input_ctx->control_context);
    }
    auto* input_ctx = static_cast<XhciInputContext32*>(input_context_);
    return &input_ctx->control_context;
}

XHCI_SLOT_CONTEXT32* XhciDevice::get_input_slot_ctx() const
{
    if (use_64_byte_ctx_)
    {
        auto* input_ctx = static_cast<XhciInputContext64*>(input_context_);
        return reinterpret_cast<XHCI_SLOT_CONTEXT32*>(&input_ctx->device_context.slot_context);
    }
    auto* input_ctx = static_cast<XhciInputContext32*>(input_context_);
    return &input_ctx->device_context.slot_context;
}

XHCI_ENDPOINT_CONTEXT32* XhciDevice::get_input_control_ep_ctx() const
{
    if (use_64_byte_ctx_)
    {
        auto* input_ctx = static_cast<XhciInputContext64*>(input_context_);
        return reinterpret_cast<XHCI_ENDPOINT_CONTEXT32*>(&input_ctx->device_context.control_ep_context);
    }
    auto* input_ctx = static_cast<XhciInputContext32*>(input_context_);
    return &input_ctx->device_context.control_ep_context;
}

XHCI_ENDPOINT_CONTEXT32* XhciDevice::get_input_ep_ctx(u8 endpoint_num) const
{
    const u8 endpoint_index = endpoint_num - 2;

    if (use_64_byte_ctx_)
    {
        auto* input_ctx = static_cast<XhciInputContext64*>(input_context_);
        return reinterpret_cast<XHCI_ENDPOINT_CONTEXT32*>(&input_ctx->device_context.ep[endpoint_index]);
    }
    auto* input_ctx = static_cast<XhciInputContext32*>(input_context_);
    return &input_ctx->device_context.ep[endpoint_index];
}

void XhciDevice::setup_add_interface(const USB_INTERFACE_DESCRIPTOR* desc)
{
    const auto iface = new XhciUsbInterface(info.slot_id, desc);
    interfaces.push_back(iface);
}

void XhciDevice::sync_input_ctx(const void* out_ctx) const
{
    if (use_64_byte_ctx_)
    {
        auto* input_ctx = static_cast<XhciInputContext64*>(input_context_);
        XHCI_DEVICE_CONTEXT64* input_device_ctx = &input_ctx->device_context;
        memcpy(input_device_ctx, out_ctx, sizeof(XHCI_DEVICE_CONTEXT64));
    }
    else
    {
        auto* input_ctx = static_cast<XhciInputContext32*>(input_context_);
        XHCI_DEVICE_CONTEXT32* input_device_ctx = &input_ctx->device_context;
        memcpy(input_device_ctx, out_ctx, sizeof(XHCI_DEVICE_CONTEXT32));
    }
}
