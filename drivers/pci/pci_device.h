// pci_device.h
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

#ifndef VESPERAOS_DRIVERS_PCI_PCI_DEVICE_H
#define VESPERAOS_DRIVERS_PCI_PCI_DEVICE_H

#include <vespera/types.h>

#include "pci.h"
#include "pci_id.h"

namespace pci {

    class PciDriver;

    /**
     * @brief Represents a single discovered PCI function.
     *
     * Created during enumeration and passed to matching drivers via probe().
     * Owns the mapped config-space pointer and the bus address.
     */
    struct pci_device {
        pci_id id;

        PCI_HEADER0* header = nullptr;

        // Shortcut fields, values are sourced from the header
        u16 vendor_id = 0;
        u16 device_id = 0;
        u8 class_code = 0;
        u8 subclass = 0;
        u8 prog_if = 0;
        u8 revision = 0;

        PciDriver* driver = nullptr;

        pci_device() = default;
        pci_device(const pci_device&) = delete;
        pci_device& operator=(const pci_device&) = delete;

        pci_device(pci_device&&) = default;
        pci_device& operator=(pci_device&&) = default;
    };

}  // namespace pci

#endif  // VESPERAOS_DRIVERS_PCI_PCI_DEVICE_H