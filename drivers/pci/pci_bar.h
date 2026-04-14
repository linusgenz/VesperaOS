// pci_bar.h
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

#ifndef VESPERAOS_DRIVERS_PCI_PCI_BAR_H
#define VESPERAOS_DRIVERS_PCI_PCI_BAR_H

#include <vespera/types.h>

#include "pci.h"

namespace pci::bar {

    /**
     * @brief Returns true if the given raw BAR value describes a 64-bit memory BAR.
     *
     * A memory BAR is 64-bit when bits [2:1] of the BAR value equal 0b10.
     * I/O BARs (bit 0 set) are never 64-bit.
     */
    [[nodiscard]] bool is_64_bit(u32 bar_value);

    /**
     * @brief Reads and decodes the full BarInfo for a given BAR index.
     *
     * @param header  Pointer to the Type-0 config-space header (MMIO mapped).
     * @param index   BAR index 0–5.
     * @return        Decoded BarInfo; is_valid == false on error.
     */
    [[nodiscard]] BarInfo read(PCI_HEADER0* header, u8 index);

}  // namespace pci::bar

#endif  // VESPERAOS_DRIVERS_PCI_PCI_BAR_H