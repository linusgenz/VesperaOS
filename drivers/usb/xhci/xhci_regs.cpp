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
#include "../../../include/log.h"

xhci_doorbell_manager::xhci_doorbell_manager(uintptr_t base) {
    m_doorbell_registers = reinterpret_cast<xhci_doorbell_register *>(base);
}

void xhci_doorbell_manager::ring_doorbell(uint8_t doorbell, uint8_t target) const
{
    m_doorbell_registers[doorbell].raw = static_cast<uint32_t>(target);
}

void xhci_doorbell_manager::ring_command_doorbell() const {
    ring_doorbell(0, XHCI_DOORBELL_TARGET_COMMAND_RING);
}

void xhci_doorbell_manager::ring_control_endpoint_doorbell(uint8_t doorbell) const {
    ring_doorbell(doorbell, XHCI_DOORBELL_TARGET_CONTROL_EP_RING);
}

xhci_extended_capability::xhci_extended_capability(volatile uint32_t *cap_ptr)
    : m_base(cap_ptr), m_next(nullptr) {
    memset(&m_entry, 0, sizeof(m_entry));
    m_entry.raw = *m_base;

    read_next_ext_caps();
}

void xhci_extended_capability::read_next_ext_caps() {
    if (m_entry.next) {
        auto next_cap_ptr = XHCI_NEXT_EXT_CAP_PTR(m_base, m_entry.next);


        m_next = new xhci_extended_capability(next_cap_ptr);
    }
}

void xhciPortRegisterManager::read_portsc_reg(xhci_portsc_register& reg) const {
    uint64_t portsc_address = m_base + m_portsc_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portsc_address);
}

void xhciPortRegisterManager::write_portsc_reg(const xhci_portsc_register& reg) const {
    uint64_t portsc_address = m_base + m_portsc_offset;
    *reinterpret_cast<volatile uint32_t*>(portsc_address) = reg.raw;
}

void xhciPortRegisterManager::read_portpmsc_reg_usb2(xhci_portpmsc_register_usb2& reg) const {
    uint64_t portpmsc_address = m_base + m_portpmsc_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmsc_address);
}

void xhciPortRegisterManager::write_portpmsc_reg_usb2(const xhci_portpmsc_register_usb2& reg) const {
    uint64_t portpmsc_address = m_base + m_portpmsc_offset;
    *reinterpret_cast<volatile uint32_t*>(portpmsc_address) = reg.raw;
}

void xhciPortRegisterManager::read_portpmsc_reg_usb3(xhci_portpmsc_register_usb3& reg) const {
    uint64_t portpmsc_address = m_base + m_portpmsc_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portpmsc_address);
}

void xhciPortRegisterManager::write_portpmsc_reg_usb3(const xhci_portpmsc_register_usb3& reg) const {
    uint64_t portpmsc_address = m_base + m_portpmsc_offset;
    *reinterpret_cast<volatile uint32_t*>(portpmsc_address) = reg.raw;
}

void xhciPortRegisterManager::read_portli_reg(xhci_portli_register& reg) const {
    uint64_t portli_address = m_base + m_portli_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(portli_address);
}

void xhciPortRegisterManager::write_portli_reg(const xhci_portli_register& reg) const {
    uint64_t portli_address = m_base + m_portli_offset;
    *reinterpret_cast<volatile uint32_t*>(portli_address) = reg.raw;
}

void xhciPortRegisterManager::read_porthlpmc_reg_usb2(xhci_porthlpmc_register_usb2& reg) const {
    uint64_t porthlpm_address = m_base + m_porthlpmc_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpm_address);
}

void xhciPortRegisterManager::write_porthlpmc_reg_usb2(const xhci_porthlpmc_register_usb2& reg) const {
    uint64_t porthlpm_address = m_base + m_porthlpmc_offset;
    *reinterpret_cast<volatile uint32_t*>(porthlpm_address) = reg.raw;
}

void xhciPortRegisterManager::read_porthlpmc_reg_usb3(xhci_porthlpmc_register_usb3& reg) const {
    uint64_t porthlpm_address = m_base + m_porthlpmc_offset;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(porthlpm_address);
}

void xhciPortRegisterManager::write_porthlpmc_reg_usb3(const xhci_porthlpmc_register_usb3& reg) const {
    uint64_t porthlpm_address = m_base + m_porthlpmc_offset;
    *reinterpret_cast<volatile uint32_t*>(porthlpm_address) = reg.raw;
}