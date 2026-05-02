// acpi_subsystem.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 13.04.26.
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

#ifndef VESPERAOS_INCLUDE_ACPI_ACPI_SUBSYSTEM_H
#define VESPERAOS_INCLUDE_ACPI_ACPI_SUBSYSTEM_H

#include <vespera/types.h>

#include "../../kernel/acpi/acpi_tables.h"

struct BootInfo;

namespace kernel::acpi {
    void early_init(const BootInfo* boot_info);

    void init();

    [[nodiscard]] u64 get_rsdp_phys();

    [[nodiscard]] FADT* get_fadt();
    [[nodiscard]] MADT_HEADER* get_madt();
    [[nodiscard]] MCFG_HEADER* get_mcfg();
    [[nodiscard]] HPET* get_hpet();

}  // namespace kernel::acpi

#endif  // VESPERAOS_INCLUDE_ACPI_ACPI_SUBSYSTEM_H
