// sys_reboot.cpp
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

#include "../../acpi/acpi.h"
#define REBOOT_MAGIC1 0xfee1dead
#define REBOOT_MAGIC2 672274793

enum RebootCmd {
    REBOOT_RESTART = 0,
    REBOOT_POWER_OFF = 1,
    REBOOT_HALT = 2
};

namespace syscalls::internal {
    int64_t sys_reboot(uint64_t magic1, uint64_t magic2, uint64_t cmd, uint64_t, uint64_t, uint64_t) {
        if (magic1 != REBOOT_MAGIC1 || magic2 != REBOOT_MAGIC2) {
            return -1;
        }

        switch (cmd) {
            case REBOOT_RESTART:
                ACPI::acpi_reboot();
                break;
            case REBOOT_POWER_OFF:
                ACPI::acpi_power_off();
                break;
            default:
                return -1;
        }

        return 0;
    }
}
