/**
 * @file reboot.c
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 06.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdint.h>
#include <sysstd.h>
#include <reboot.h>

#define REBOOT_MAGIC1 0xfee1dead
#define REBOOT_MAGIC2 672274793

int reboot(reboot_mode_t mode) {
    switch (mode) {
    case REBOOT_MODE_RESTART:
    case REBOOT_MODE_POWER_OFF:
    case REBOOT_MODE_HALT:
        break;
    default:
        return -1;
    }

    return sys_reboot(REBOOT_MAGIC1, REBOOT_MAGIC2, (uint64_t)mode, 0, 0, 0);
}