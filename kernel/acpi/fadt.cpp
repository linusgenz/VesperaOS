// fadt.cpp
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 05.08.25.
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

#include "acpi_manager.h"
#include <log.h>
#include "../cpu/io.h"
#include <kernel/memory.h>

namespace ACPI {

    static uint8_t slp_typa = 0;
    static uint8_t slp_typb = 0;

    void parse_s5(const uint8_t* dsdt, size_t length) {
        for (size_t i = 0; i < length - 6; i++) {
            if (dsdt[i] == '_' && dsdt[i+1] == 'S' && dsdt[i+2] == '5' && dsdt[i+3] == '_') {
                // Expect AML: NameOp (0x08), PkgOp (0x12)
                if (dsdt[i+4] != 0x12) continue; // Not a PackageOp

                uint8_t pkg_len = dsdt[i+5];
                uint8_t elem_count = dsdt[i+6];
                if (elem_count < 2) continue;

                if (dsdt[i+7] == 0x0A) slp_typa = dsdt[i+8]; // BytePrefix
                else slp_typa = dsdt[i+7];

                if (dsdt[i+9] == 0x0A) slp_typb = dsdt[i+10];
                else slp_typb = dsdt[i+9];

                return;
            }
        }
    }

    void parse_fadt() {
        FADT* fadt = TableManager::get_fadt();
        if (!fadt) return;

        uint64_t dsdt_phys = fadt->x_dsdt != 0 ? fadt->x_dsdt : fadt->dsdt;
        if (dsdt_phys == 0) return;

        const auto* header = static_cast<SDTHeader*>(phys_to_virt(dsdt_phys));
        auto* dsdt = static_cast<uint8_t*>(phys_to_virt(dsdt_phys));
        size_t length = header->length;

        parse_s5(dsdt, length);
    }

    void acpi_power_off() {
        FADT* fadt = TableManager::get_fadt();
        if (!fadt || slp_typa == 0) return;

        uint16_t port = fadt->pm1a_control_block;
        uint16_t value = (slp_typa << 10) | (1 << 13); // SLP_TYP | SLP_EN

        outw(port, value);

        if (fadt->pm1b_control_block)
            outw(fadt->pm1b_control_block, value);

        Log::debug("acpi_power_off");
        while (true) {
            asm volatile ("cli; hlt");
        }
    }

    void acpi_reboot() {
        FADT* fadt = TableManager::get_fadt();
        if (!fadt) return;

        // Use ACPI reset register (if supported)
        if (fadt->reset_reg.address_space == 1 && fadt->reset_reg.address != 0) {
            outb(fadt->reset_reg.address, fadt->reset_value);
        }

        Log::debug("acpi_reboot");
        while (true) {
            asm volatile ("cli; hlt");
        }
    }

}
