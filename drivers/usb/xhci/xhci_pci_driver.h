// xhci_pci_driver.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 14.04.26.
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

#ifndef VESPERAOS_DRIVERS_USB_XHCI_XHCI_PCI_DRIVER_H
#define VESPERAOS_DRIVERS_USB_XHCI_XHCI_PCI_DRIVER_H

#include <drivers/pci/pci_driver.h>

namespace usb {

    /**
     * @brief PCI binding for the xHCI (USB 3) host controller.
     *
     * Matches any device with class 0x0C / subclass 0x03 / prog_if 0x30.
     * Also accepts UHCI (0x00), OHCI (0x10), and EHCI (0x20) entries via
     * the same class match so those remain visible to lspci even if we
     * don't yet start a driver for them.
     *
     * One instance is registered at startup via PCI_DRIVER_REGISTER.
     */
    class XhciPciDriver final : public pci::PciDriver {
    public:
        XhciPciDriver() = default;
        ~XhciPciDriver() override = default;

        [[nodiscard]] const char* name() const override { return "xhci"; }

        [[nodiscard]] int probe(pci::pci_device& dev) override;
        void remove(pci::pci_device& dev) override;

    protected:
        [[nodiscard]] const pci::pci_class_match* class_match() const override;
    };

} // namespace usb

#endif // VESPERAOS_DRIVERS_USB_XHCI_XHCI_PCI_DRIVER_H