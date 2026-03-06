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

#include "../../../include/log.h"
#include "xhci_common.h"

XhciDoorbellManager::XhciDoorbellManager(uintptr_t base)
    : doorbell_registers_(reinterpret_cast<XHCI_DOORBELL_REGISTER*>(base)) {
}

void XhciDoorbellManager::ring_doorbell(uint8_t doorbell, uint8_t target) const {
    doorbell_registers_[doorbell].raw = static_cast<uint32_t>(target);
}

void XhciDoorbellManager::ring_command_doorbell() const {
    ring_doorbell(0, XHCI_DOORBELL_TARGET_COMMAND_RING);
}

void XhciDoorbellManager::ring_control_endpoint_doorbell(uint8_t doorbell) const {
    ring_doorbell(doorbell, XHCI_DOORBELL_TARGET_CONTROL_EP_RING);
}

XhciExtendedCapability::XhciExtendedCapability(volatile uint32_t* cap_ptr)
    : base_(cap_ptr)
    , next_(nullptr) {
    memset(&entry_, 0, sizeof(entry_));
    entry_.raw = *base_;

    read_next_ext_caps();
}

void XhciExtendedCapability::read_next_ext_caps() {
    if (entry_.next) {
        auto next_cap_ptr = XHCI_NEXT_EXT_CAP_PTR(base_, entry_.next);

        next_ = new XhciExtendedCapability(next_cap_ptr);
    }
}

void XhciPortRegisterManager::read_portsc_reg(XHCI_PORTSC_REGISTER& reg) const {
    uint64_t portsc_address = base_ + portsc_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portsc_address);
}

void XhciPortRegisterManager::write_portsc_reg(const XHCI_PORTSC_REGISTER& reg) const {
    uint64_t portsc_address = base_ + portsc_offset_;
    *reinterpret_cast<volatile uint32_t*>(portsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portpmsc_reg_usb2(XHCI_PORTPMSC_REGISTER_USB2& reg) const {
    uint64_t portpmsc_address = base_ + portpmsc_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmsc_address);
}

void XhciPortRegisterManager::write_portpmsc_reg_usb2(const XHCI_PORTPMSC_REGISTER_USB2& reg) const {
    uint64_t portpmsc_address = base_ + portpmsc_offset_;
    *reinterpret_cast<volatile uint32_t*>(portpmsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portpmsc_reg_usb3(XHCI_PORTPMSC_REGISTER_USB3& reg) const {
    uint64_t portpmsc_address = base_ + portpmsc_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmsc_address);
}

void XhciPortRegisterManager::write_portpmsc_reg_usb3(const XHCI_PORTPMSC_REGISTER_USB3& reg) const {
    uint64_t portpmsc_address = base_ + portpmsc_offset_;
    *reinterpret_cast<volatile uint32_t*>(portpmsc_address) = reg.raw;
}

void XhciPortRegisterManager::read_portli_reg(XHCI_PORTLI_REGISTER& reg) const {
    uint64_t portli_address = base_ + portli_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portli_address);
}

void XhciPortRegisterManager::write_portli_reg(const XHCI_PORTLI_REGISTER& reg) const {
    uint64_t portli_address = base_ + portli_offset_;
    *reinterpret_cast<volatile uint32_t*>(portli_address) = reg.raw;
}

void XhciPortRegisterManager::read_porthlpmc_reg_usb2(XHCI_PORTHLPMC_REGISTER_USB2& reg) const {
    uint64_t porthlpm_address = base_ + porthlpmc_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpm_address);
}

void XhciPortRegisterManager::write_porthlpmc_reg_usb2(const XHCI_PORTHLPMC_REGISTER_USB2& reg) const {
    uint64_t porthlpm_address = base_ + porthlpmc_offset_;
    *reinterpret_cast<volatile uint32_t*>(porthlpm_address) = reg.raw;
}

void XhciPortRegisterManager::read_porthlpmc_reg_usb3(XHCI_PORTHLPMC_REGISTER_USB3& reg) const {
    uint64_t porthlpm_address = base_ + porthlpmc_offset_;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpm_address);
}

void XhciPortRegisterManager::write_porthlpmc_reg_usb3(const XHCI_PORTHLPMC_REGISTER_USB3& reg) const {
    uint64_t porthlpm_address = base_ + porthlpmc_offset_;
    *reinterpret_cast<volatile uint32_t*>(porthlpm_address) = reg.raw;
}