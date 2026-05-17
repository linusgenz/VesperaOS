// intel_blt_pci_driver.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 16.05.26.
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

#ifndef VESPERAOS_DRIVERS_GPU_INTEL_INTEL_BLT_PCI_DRIVER_H
#define VESPERAOS_DRIVERS_GPU_INTEL_INTEL_BLT_PCI_DRIVER_H

#include <drivers/pci/pci_driver.h>

namespace blt {
    class IntelBlt;

    /**
     * @brief PCI driver that probes and starts the Intel BLT render engine.
     *
     * Registered at link time via @ref PCI_DRIVER_REGISTER. When the PCI
     * subsystem finds a matching Intel display controller, @ref probe allocates
     * an @ref IntelBlt instance, calls @ref IntelBlt::start_device, and stores
     * a pointer to it in the device's driver-data slot.
     *
     * @note Only one Intel GPU is expected per system. A second probe call logs
     *       a warning and returns -1.
     * @see blt::IntelBlt
     */
    class IntelBltPciDriver final : public pci::PciDriver {
       public:
        IntelBltPciDriver() = default;
        ~IntelBltPciDriver() override = default;

        IntelBltPciDriver(const IntelBltPciDriver&) = delete;
        IntelBltPciDriver& operator=(const IntelBltPciDriver&) = delete;

        [[nodiscard]] const char* name() const override {
            return "intel-blt";
        }

        /**
         * @brief Probes an Intel display controller and starts the BLT engine.
         *
         * Enables Bus Master and Memory Space in the PCI command register,
         * allocates an @ref IntelBlt instance, and calls
         * @ref IntelBlt::start_device with the current screen resolution.
         * Returns 0 on success so the PCI core marks the device as claimed.
         *
         * @return 0   on success.
         * @return -1  if the device function is not 0, if a second GPU is
         *             detected, or if @ref IntelBlt::start_device fails.
         *
         * @warning @p dev must have a valid BAR5 (MMIO) mapping before probe
         *          is called — the PCI core is expected to have assigned BARs.
         */
        [[nodiscard]] int probe(pci::pci_device& dev) override;

        /**
         * @brief Releases resources acquired during @ref probe.
         *
         * Destroys the @ref IntelBlt instance. Hot-unplug of an integrated GPU
         * is not a realistic scenario, but the method is provided for
         * completeness.
         */
        void remove(pci::pci_device& dev) override;

       protected:
        [[nodiscard]] const pci::pci_device_match* id_match() const override;

       private:
        IntelBlt* driver_{nullptr};  ///< Owning pointer; null until a device is successfully probed.
    };

}  // namespace blt

#endif  // VESPERAOS_DRIVERS_GPU_INTEL_INTEL_BLT_PCI_DRIVER_H
