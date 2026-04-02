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

#include <vespera/cpu/io.h>
#include <vespera/log.h>
#include <acpi/acpi.h>

namespace acpi {
    void acpi_power_off() {
        ACPI_STATUS status = AcpiEnterSleepStatePrep(ACPI_STATE_S5);
        if (ACPI_FAILURE(status)) {
            Log::error("acpi_power_off: AcpiEnterSleepStatePrep failed: %u", status);
        }

        asm volatile("cli");

        status = AcpiEnterSleepState(ACPI_STATE_S5);
        if (ACPI_FAILURE(status)) {
            Log::error("acpi_power_off: AcpiEnterSleepState failed: %u", status);
        }

        Log::error("acpi_power_off: system did not power off");
        while (true) {
            asm volatile("cli; hlt");
        }
    }

    void acpi_reboot() {
        ACPI_STATUS status = AcpiReset();
        if (ACPI_FAILURE(status)) {
            Log::error("acpi_reboot: AcpiReset failed: %u, trying keyboard controller fallback", status);
            outb(0x64, 0xFE);
        }

        Log::error("acpi_reboot: system did not reboot");
        while (true) {
            asm volatile("cli; hlt");
        }
    }

}  // namespace acpi