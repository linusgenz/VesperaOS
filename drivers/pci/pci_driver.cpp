// pci_driver.cpp
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

#include <drivers/pci/pci_driver.h>
#include <vespera/log.h>

namespace pci {

    bool PciDriver::matches(const pci_device& dev) const {
        const pci_class_match* cls = class_match();
        if (cls) {
            for (;
                 !(cls->class_code == PCI_CLASS_ANY && cls->subclass == PCI_CLASS_ANY && cls->prog_if == PCI_CLASS_ANY);
                 ++cls) {
                const bool class_ok = (cls->class_code == PCI_CLASS_ANY) || (cls->class_code == dev.class_code);
                const bool sub_ok = (cls->subclass == PCI_CLASS_ANY) || (cls->subclass == dev.subclass);
                const bool pif_ok = (cls->prog_if == PCI_CLASS_ANY) || (cls->prog_if == dev.prog_if);
                if (class_ok && sub_ok && pif_ok) return true;
            }
        }

        const pci_device_match* ids = id_match();
        if (ids) {
            for (; !(ids->vendor_id == PCI_ID_ANY && ids->device_id == PCI_ID_ANY); ++ids) {
                const bool vendor_ok = (ids->vendor_id == PCI_ID_ANY) || (ids->vendor_id == dev.vendor_id);
                const bool device_ok = (ids->device_id == PCI_ID_ANY) || (ids->device_id == dev.device_id);
                if (vendor_ok && device_ok) return true;
            }
        }

        return false;
    }

    namespace driver_registry {

        extern "C" PciDriver* __start_pci_drivers[];
        extern "C" PciDriver* __stop_pci_drivers[];

        void init_drivers() {
            for (PciDriver** drv = __start_pci_drivers; drv < __stop_pci_drivers; ++drv) {
                register_driver(*drv);
            }
        }

        constexpr usize MAX_DRIVERS = 64;

        static PciDriver* g_drivers[MAX_DRIVERS] = {};
        static usize g_driver_count = 0;

        void register_driver(PciDriver* driver) {
            if (!driver) return;
            if (g_driver_count >= MAX_DRIVERS) {
                Log::error("pci: driver_registry: table full, cannot register '%s'", driver->name());
                return;
            }
            g_drivers[g_driver_count++] = driver;
        }

        bool bind(pci_device& dev) {
            for (usize i = 0; i < g_driver_count; ++i) {
                PciDriver* drv = g_drivers[i];
                if (!drv) continue;

                if (drv->matches(dev)) {
                    const int err = drv->probe(dev);
                    if (err == 0) {
                        dev.driver = drv;
                        return true;
                    }
                    Log::warning(
                        "pci: driver '%s' probe failed for %04x:%02x:%02x.%x (err %d)",
                        drv->name(),
                        dev.id.domain,
                        dev.id.bus,
                        dev.id.device,
                        dev.id.function,
                        err
                    );
                }
            }
            return false;
        }

    }  // namespace driver_registry

}  // namespace pci
