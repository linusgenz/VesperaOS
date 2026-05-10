// pci_id.h
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

#ifndef VESPERAOS_DRIVERS_PCI_PCI_ID_H
#define VESPERAOS_DRIVERS_PCI_PCI_ID_H

#include <vespera/types.h>

namespace pci {

    /**
     * @brief Identifies the bus-level location of a PCI function.
     */
    struct pci_id {
        u16 domain = 0;  // PCI domain (segment group from MCFG)
        u8 bus = 0;
        u8 device = 0;    // 0–31
        u8 function = 0;  // 0–7
    };

    /**
     * @brief Class/subclass/prog_if triple that identifies a PCI function type.
     *
     * Used for driver matching. Set any field to PCI_CLASS_ANY (0xFF) to
     * act as a wildcard.
     */
    struct pci_class_match {
        u8 class_code = 0xFF;
        u8 subclass = 0xFF;
        u8 prog_if = 0xFF;
    };

    /**
     * @brief Vendor/device ID pair for ID-based driver matching.
     *
     * Set either field to @ref PCI_ID_ANY (0xFFFF) to match any value.
     */
    struct pci_device_match {
        u16 vendor_id = 0xFFFF;
        u16 device_id = 0xFFFF;
    };

    constexpr u8 PCI_CLASS_ANY = 0xFF;  ///< Wildcard for @ref pci_class_match fields.
    constexpr u16 PCI_ID_ANY = 0xFFFF;  ///< Wildcard for @ref pci_device_match fields.

}  // namespace pci

#endif  // VESPERAOS_DRIVERS_PCI_PCI_ID_H
