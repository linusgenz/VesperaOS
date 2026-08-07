// xhci_pci_driver.cpp
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

#include "xhci_pci_driver.h"

#include <drivers/usb/usb_manager.h>
#include <pci/msix.h>
#include <vespera/devices/device_manager.h>
#include <vespera/interrupts.h>
#include <vespera/log.h>
#include <vespera/realm/realm_types.h>
#include <vespera/unit/unit_manager.h>
#include <vespera/unit_config.h>

#include "xhci.h"

namespace usb {

    static constexpr pci::pci_class_match XHCI_CLASS_MATCH[] = {
        { .class_code = 0x0C, .subclass = 0x03, .prog_if = 0x30 },
        // we recognise UHCI / OHCI / EHCI but skip them in probe()
        { .class_code = 0x0C, .subclass = 0x03, .prog_if = 0x00 },
        { .class_code = 0x0C, .subclass = 0x03, .prog_if = 0x10 },
        { .class_code = 0x0C, .subclass = 0x03, .prog_if = 0x20 },
        { pci::PCI_CLASS_ANY, pci::PCI_CLASS_ANY, pci::PCI_CLASS_ANY },
    };

    const pci::pci_class_match* XhciPciDriver::class_match() const {
        return XHCI_CLASS_MATCH;
    }

    struct xhci_probe_args {
        volatile pci::PCI_DEVICE_HEADER* pci_header;
        u8                      vector;
        u8                      bus_number;
        pci::pci_device        dev;
    };

    static void xhci_unit_entry(void* arg) {
        auto* args = static_cast<xhci_probe_args*>(arg);

        char name[16];
        DeviceManager::alloc_unique_device_name("xhci", name, sizeof(name));

        auto* driver = new XhciDriver(args->vector, name, args->bus_number, klib::move(args->dev));
        if (!driver->init_device(args->pci_header)) {
            Log::error("xhci: init_device failed for bus %u", args->bus_number);
            UsbManager::notify_controller_ready();
            delete driver;
            delete args;
            return;
        }
        driver->start_device();
        delete args;
    }

    static AtomicU8 g_next_bus_number;

    int XhciPciDriver::probe(pci::pci_device& dev) {
        // Only handle xHCI
        if (dev.prog_if != 0x30) {
            Log::debug("xhci: skipping non-xHCI USB controller (prog_if=%02x)", dev.prog_if);
            return -1;
        }

        auto* pci_hdr = &dev.header->header;

        // Enable Bus-Master + Memory Space; disable legacy INTx
        u16 command = pci::pci_read16(pci_hdr, 0x04);
        command |= (1u << 2) | (1u << 1); // Bus Master + Memory Space
        command |= (1u << 10);            // Disable INTx
        pci::pci_write16(pci_hdr, 0x04, command);

        const u8 vector = kernel::interrupts::get_free_vector();
        if (!pci::try_enable_msi_or_msix(dev.header, vector)) {
            Log::error("xhci: could not enable MSI/MSI-X for %04x:%02x:%02x.%x",
                dev.id.domain, dev.id.bus, dev.id.device, dev.id.function);
            return -1;
        }

        const u8 bus_number = g_next_bus_number++;
        UsbManager::increment_expected_count();

        auto* args = new xhci_probe_args{
            .pci_header = pci_hdr,
            .vector     = vector,
            .bus_number = bus_number,
            .dev        = (klib::move(dev))
        };

        char unit_name[32];
        snprintf(unit_name, sizeof(unit_name), "xhci%u", bus_number);

        const UnitConfig cfg = {
            .name               = unit_name,
            .cpu_id             = 2,
            .priority           = 5,
            .stack_size         = 0x4000,
            .initial_handles    = nullptr,
            .initial_handle_count = 0,
            .is_idle            = false,
            .is_user            = false,
            .user_stack_size    = 0,
        };

        const Unit* unit = UnitManager::create(kernel::realm::REALM_DRIVER, xhci_unit_entry, args, &cfg);
        if (!unit) {
            Log::error("xhci: failed to create unit for bus %u", bus_number);
            UsbManager::notify_controller_ready();
            delete args;
            return -1;
        }

        return 0;
    }

    void XhciPciDriver::remove(pci::pci_device& /*dev*/) {
        // Hot-unplug not yet supported
    }

    static XhciPciDriver g_xhci_pci_driver;
   // PCI_DRIVER_REGISTER(g_xhci_pci_driver);

} // namespace usb