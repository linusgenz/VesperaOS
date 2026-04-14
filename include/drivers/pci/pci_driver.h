// pci_driver.h
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

#ifndef VESPERAOS_DRIVERS_PCI_PCI_DRIVER_H
#define VESPERAOS_DRIVERS_PCI_PCI_DRIVER_H

#include <vespera/types.h>

#include "../../../drivers/pci/pci_device.h"
#include "../../../drivers/pci/pci_id.h"

namespace pci {

    /**
     * @brief Abstract base class for all PCI drivers.
     *
     * Subclasses declare their match table and implement probe() / remove().
     *
     * Registration example (in the driver's .cpp):
     *
     *   static XhciPciDriver g_xhci_driver;
     *   PCI_DRIVER_REGISTER(g_xhci_driver);
     *
     * Matching is attempted in registration order.  The first driver whose
     * matches() returns true claims the device and its probe() is called.
     */
    class PciDriver {
       public:
        PciDriver() = default;
        virtual ~PciDriver() = default;

        PciDriver(const PciDriver&) = delete;
        PciDriver& operator=(const PciDriver&) = delete;

        /**
         * @brief Human-readable name shown in lspci / kernel logs.
         */
        [[nodiscard]] virtual const char* name() const = 0;

        /**
         * @brief Returns true if this driver handles the given device.
         *
         * The default implementation checks class_match() and id_match()
         * against the device's fields.  Override for custom logic.
         */
        [[nodiscard]] virtual bool matches(const pci_device& dev) const;

        /**
         * @brief Called by the binding layer when a matching device is found.
         *
         * Implementations should enable Bus-Master/Memory-Space as needed,
         * allocate an interrupt vector, and start any required kernel units.
         *
         * @return 0 on success, negative error code on failure.
         */
        [[nodiscard]] virtual int probe(pci_device& dev) = 0;

        /**
         * @brief Called when a device is removed or the driver is unloaded.
         *
         * May be a no-op for drivers that do not support hot-unplug.
         */
        virtual void remove(pci_device& dev) = 0;

       protected:
        /**
         * @brief Override to provide a list of class/subclass/prog_if triples.
         *
         * Return nullptr to skip class-based matching (use id_match instead).
         * The list must be terminated by an entry with all fields == PCI_CLASS_ANY.
         */
        [[nodiscard]] virtual const pci_class_match* class_match() const {
            return nullptr;
        }

        /**
         * @brief Override to provide a list of vendor/device ID pairs.
         *
         * Return nullptr to skip ID-based matching (use class_match instead).
         * The list must be terminated by an entry with both fields == PCI_ID_ANY.
         */
        [[nodiscard]] virtual const pci_device_match* id_match() const {
            return nullptr;
        }
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Driver registry
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Static registry of all PCI drivers.
     *
     * Drivers register themselves at startup via PCI_DRIVER_REGISTER().
     * The enumerator calls driver_registry::bind() for each discovered device.
     */
    namespace driver_registry {

        /**
         * @brief Register a driver instance.
         *
         * Usually called through PCI_DRIVER_REGISTER rather than directly.
         * Safe to call before the scheduler is up (uses a fixed-size table).
         */
        void register_driver(PciDriver* driver);

        /**
         * @brief Try to bind a discovered device to a registered driver.
         *
         * Iterates registered drivers in order, calls matches(), and on the
         * first match calls probe().  Sets dev.driver on success.
         *
         * @return true if a driver claimed the device.
         */
        bool bind(pci_device& dev);

    }  // namespace driver_registry

}  // namespace pci

/**
 * @brief Register a statically-allocated PciDriver instance.
 *
 * Place this in the driver's .cpp file, at file scope, after the driver
 * class definition:
 *
 *   static XhciPciDriver g_xhci_driver;
 *   PCI_DRIVER_REGISTER(g_xhci_driver);
 *
 * Uses a constructor-priority attribute to run before main() / kernel_main().
 */
#define PCI_DRIVER_REGISTER(instance)                                     \
    __attribute__((constructor)) static void _pci_register_##instance() { \
        pci::driver_registry::register_driver(&(instance));               \
    }

#endif  // VESPERAOS_DRIVERS_PCI_PCI_DRIVER_H