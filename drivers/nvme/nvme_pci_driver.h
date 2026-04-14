// nvme_pci_driver.h
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

#ifndef VESPERAOS_DRIVERS_NVME_NVME_PCI_DRIVER_H
#define VESPERAOS_DRIVERS_NVME_NVME_PCI_DRIVER_H

#include <drivers/pci/pci_driver.h>

namespace nvme {

    class NvmePciDriver final : public pci::PciDriver {
       public:
        NvmePciDriver() = default;
        ~NvmePciDriver() override = default;

        NvmePciDriver(const NvmePciDriver&) = delete;
        NvmePciDriver& operator=(const NvmePciDriver&) = delete;

        [[nodiscard]] const char* name() const override {
            return "nvme";
        }

        [[nodiscard]] int probe(pci::pci_device& dev) override;
        void remove(pci::pci_device& dev) override;

       protected:
        [[nodiscard]] const pci::pci_class_match* class_match() const override;
    };

}  // namespace nvme

#endif  // VESPERAOS_DRIVERS_NVME_NVME_PCI_DRIVER_H