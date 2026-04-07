// xhci_regs.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 22.07.25.
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

#include "xhci_regs.h"

#include "xhci_common.h"
#include <klib/string.h>

XhciDoorbellManager::XhciDoorbellManager(const uptr base)
    : doorbell_registers_(reinterpret_cast<XHCI_DOORBELL_REGISTER*>(base)) {
}

void XhciDoorbellManager::ring_doorbell(const u8 doorbell, const u8 target) const {
    doorbell_registers_[doorbell].raw = static_cast<u32>(target);
}

void XhciDoorbellManager::ring_command_doorbell() const {
    ring_doorbell(0, XHCI_DOORBELL_TARGET_COMMAND_RING);
}

void XhciDoorbellManager::ring_control_endpoint_doorbell(const u8 doorbell) const {
    ring_doorbell(doorbell, XHCI_DOORBELL_TARGET_CONTROL_EP_RING);
}

XhciExtendedCapability::XhciExtendedCapability(volatile u32* cap_ptr)
    : base_(cap_ptr)
    , next_(nullptr) {
    memset(&entry_, 0, sizeof(entry_));
    entry_.raw = *base_;

    read_next_ext_caps();
}

void XhciExtendedCapability::read_next_ext_caps() {
    if (entry_.next) {
        const auto next_cap_ptr = XHCI_NEXT_EXT_CAP_PTR(base_, entry_.next);

        next_ = new XhciExtendedCapability(next_cap_ptr);
    }
}

void XhciPortRegisterManager::read_portsc_reg(XHCI_PORTSC_REGISTER& reg) const {
    const u64 portsc_address = base_ + portsc_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(portsc_address);
}

void XhciPortRegisterManager::write_portsc_reg(const XHCI_PORTSC_REGISTER& reg) const {
    const u64 portsc_address = base_ + portsc_offset_;
    *reinterpret_cast<volatile u32*>(portsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portpmsc_reg_usb2(XHCI_PORTPMSC_REGISTER_USB2& reg) const {
    const u64 portpmsc_address = base_ + portpmsc_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(portpmsc_address);
}

void XhciPortRegisterManager::write_portpmsc_reg_usb2(const XHCI_PORTPMSC_REGISTER_USB2& reg) const {
    const u64 portpmsc_address = base_ + portpmsc_offset_;
    *reinterpret_cast<volatile u32*>(portpmsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portpmsc_reg_usb3(XHCI_PORTPMSC_REGISTER_USB3& reg) const {
    const u64 portpmsc_address = base_ + portpmsc_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(portpmsc_address);
}

void XhciPortRegisterManager::write_portpmsc_reg_usb3(const XHCI_PORTPMSC_REGISTER_USB3& reg) const {
    const u64 portpmsc_address = base_ + portpmsc_offset_;
    *reinterpret_cast<volatile u32*>(portpmsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portli_reg(XHCI_PORTLI_REGISTER& reg) const {
    const u64 portli_address = base_ + portli_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(portli_address);
}

void XhciPortRegisterManager::write_portli_reg(const XHCI_PORTLI_REGISTER& reg) const {
    const u64 portli_address = base_ + portli_offset_;
    *reinterpret_cast<volatile u32*>(portli_address) = reg.raw;
}

void XhciPortRegisterManager::read_porthlpmc_reg_usb2(XHCI_PORTHLPMC_REGISTER_USB2& reg) const {
    const u64 porthlpm_address = base_ + porthlpmc_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(porthlpm_address);
}

void XhciPortRegisterManager::write_porthlpmc_reg_usb2(const XHCI_PORTHLPMC_REGISTER_USB2& reg) const {
    const u64 porthlpm_address = base_ + porthlpmc_offset_;
    *reinterpret_cast<volatile u32*>(porthlpm_address) = reg.raw;
}

void XhciPortRegisterManager::read_porthlpmc_reg_usb3(XHCI_PORTHLPMC_REGISTER_USB3& reg) const {
    const u64 porthlpm_address = base_ + porthlpmc_offset_;
    reg.raw = *reinterpret_cast<volatile u32*>(porthlpm_address);
}

void XhciPortRegisterManager::write_porthlpmc_reg_usb3(const XHCI_PORTHLPMC_REGISTER_USB3& reg) const {
    const u64 porthlpm_address = base_ + porthlpmc_offset_;
    *reinterpret_cast<volatile u32*>(porthlpm_address) = reg.raw;
}