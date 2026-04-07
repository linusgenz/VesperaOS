// power.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 01.04.26.
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
#ifndef UAPI_VESPERA_DEV_POWER_H
#define UAPI_VESPERA_DEV_POWER_H

#include <vespera/types.h>
#include <vespera/ioctl.h>

#define BAT_STATE_DISCHARGING  (1 << 0)
#define BAT_STATE_CHARGING     (1 << 1)
#define BAT_STATE_CRITICAL     (1 << 2)

typedef struct battery_status {
    u32  state;               // BAT_STATE_* flags
    u32  present_rate;        // current draw in mW (0xFFFFFFFF = unknown)
    u32  remaining_capacity;  // mWh remaining (0xFFFFFFFF = unknown)
    u32  present_voltage;     // mV
    u8   percent;             // 0–100  (255 = unknown)
    u8   present;             // 1 if battery is physically present
    u8   _pad[2];
} battery_status_t;

typedef struct battery_info {
    u32  design_capacity;     // mWh
    u32  last_full_capacity;  // mWh
    u32  design_voltage;      // mV
    u32  capacity_warning;    // mWh – ACPI low-battery threshold
    char model[32];
    char serial[32];
    char type[8];             // e.g. "LION", "NIMH"
    char oem[16];
} battery_info_t;


// /dev/bat*
#define IOCTL_BAT_GET_STATUS IOR('B', 0x01, battery_status_t)
#define IOCTL_BAT_GET_INFO   IOR('B', 0x02, battery_info_t)

// /dev/power
#define IOCTL_POWER_SHUTDOWN IO('P', 0x01)
#define IOCTL_POWER_REBOOT   IO('P', 0x02)
#define IOCTL_POWER_GET_COUNT IOR('P', 0x03, uint32_t)

#endif // UAPI_VESPERA_DEV_POWER_H
