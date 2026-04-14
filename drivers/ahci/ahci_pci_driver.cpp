// ahci_pci_driver.cpp
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

#include "ahci_pci_driver.h"

#include <vespera/log.h>

#include "../pci/pci.h"
#include "ahci.h"

namespace ahci {

    static constexpr pci::pci_class_match AHCI_CLASS_MATCH[] = {
        { .class_code = 0x01, .subclass = 0x06, .prog_if = 0x01 },  // AHCI 1.0
        { pci::PCI_CLASS_ANY, pci::PCI_CLASS_ANY, pci::PCI_CLASS_ANY },
    };

    const pci::pci_class_match* AhciPciDriver::class_match() const {
        return AHCI_CLASS_MATCH;
    }

    int AhciPciDriver::probe(pci::pci_device& dev) {
        if (dev.id.function != 0) {
            Log::debug("ahci: skipping function %u on %04x:%02x:%02x",
                dev.id.function, dev.id.domain, dev.id.bus, dev.id.device);
            return -1;
        }

        auto* pci_hdr = &dev.header->header;

        // Enable Bus-Master + Memory Space; disable legacy INTx
        u16 command = pci::pci_read16(pci_hdr, 0x04);
        command |= (1u << 2) | (1u << 1);  // Bus Master + Memory Space
        command |= (1u << 10);             // Disable INTx
        pci::pci_write16(pci_hdr, 0x04, command);

        auto* driver = new AhciDriver(pci_hdr);
        if (!driver->has_active_ports()) {
            Log::warning("ahci: no active ports on %04x:%02x:%02x.%x — freeing driver",
                dev.id.domain, dev.id.bus, dev.id.device, dev.id.function);
            delete driver;
            return -1;
        }

        return 0;
    }

    void AhciPciDriver::remove(pci::pci_device& /*dev*/) {
        // Hot-unplug not yet supported
    }

    static AhciPciDriver g_ahci_pci_driver;
    PCI_DRIVER_REGISTER(g_ahci_pci_driver);

}  // namespace ahci