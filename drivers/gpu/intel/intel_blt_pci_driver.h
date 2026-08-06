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
    class IntelGpuDevice;
    class IntelBcs;
    class IntelRcs;

    /**
     * @brief PCI driver that probes the Intel iGPU and constructs one
     *        IntelGpuDevice plus its two engines (IntelBcs, IntelRcs).
     *
     * Registered at link time via @ref PCI_DRIVER_REGISTER. When the PCI
     * subsystem finds a matching Intel display controller, @ref probe
     * constructs a single @ref IntelGpuDevice (BAR0 mapping + GGTT), then
     * constructs @ref IntelBcs and @ref IntelRcs on top of it — both engines
     * are peers borrowing the same device, neither owns the other.
     *
     * @note Only one Intel GPU is expected per system. A second probe call
     *       logs a warning and returns -1.
     * @see blt::IntelGpuDevice
     * @see blt::IntelBcs
     * @see blt::IntelRcs
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
         * @brief Probes an Intel display controller and constructs the
         *        device + both engines.
         *
         * Enables Bus Master and Memory Space in the PCI command register,
         * constructs an @ref IntelGpuDevice (which maps BAR0 and sets up the
         * GGTT), then constructs @ref IntelBcs and @ref IntelRcs on top of
         * it. BCS failure aborts the whole probe (BCS currently owns display
         * output); RCS failure only logs a warning so the system still boots
         * with BCS-only 2D acceleration.
         *
         * @return 0   on success.
         * @return -1  if the device function is not 0, if a second GPU is
         *             detected, if the device (BAR0/GGTT) fails to init, or
         *             if IntelBcs::init_device() fails.
         *
         * @warning @p dev must have a valid BAR5 (MMIO) mapping before probe
         *          is called — the PCI core is expected to have assigned BARs.
         */
        [[nodiscard]] int probe(pci::pci_device& dev) override;

        /**
         * @brief Releases resources acquired during @ref probe.
         *
         * Destroys engines before the device they borrow from, then the
         * device itself. Hot-unplug of an integrated GPU is not a realistic
         * scenario, but the method is provided for completeness.
         */
        void remove(pci::pci_device& dev) override;

       protected:
        [[nodiscard]] const pci::pci_device_match* id_match() const override;

       private:
        IntelGpuDevice* device_{nullptr};  ///< Owns BAR0 mapping + GGTT; constructed first, destroyed last.
        IntelBcs* bcs_{nullptr};           ///< Peer engine; owns display output.
        IntelRcs* rcs_{nullptr};           ///< Peer engine; best-effort, may be null if init fails.
    };

}  // namespace blt

#endif  // VESPERAOS_DRIVERS_GPU_INTEL_INTEL_BLT_PCI_DRIVER_H
