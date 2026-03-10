// ioctl_smart.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 09.03.26.
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
#ifndef VESPERAOS_IOCTL_SMART_H
#define VESPERAOS_IOCTL_SMART_H

#include <vespera/types.h>

#define IOCTL_SMART_GET_RAW     0x5301
#define IOCTL_SMART_GET_COMMON  0x5302
#define IOCTL_SMART_GET_NVME    0x5303
#define IOCTL_SMART_GET_ATA     0x5304

typedef enum SmartDriverType {
    SMART_DRIVER_UNKNOWN = 0,
    SMART_DRIVER_NVME    = 1,
    SMART_DRIVER_ATA     = 2,
} SmartDriverType;

typedef struct SmartCommon {
    SmartDriverType driver_type;
    u8  temperature_celsius;
    u64 power_on_hours;
    u8  health_ok;
    u8  critical_warning_raw;
} SmartCommon;

typedef struct SmartNvme {
    u8  critical_warning_raw;
    u8  available_spare;            // 0-100%
    u8  available_spare_threshold;  // 0-100%
    u8  percentage_used;            // 0-255%
    u8  temperature_celsius;
    u16 temperature_sensor[8];      // Kelvin, 0 = nicht vorhanden

    u64 data_units_read;
    u64 data_units_written;
    u64 host_read_commands;
    u64 host_write_commands;
    u64 controller_busy_time_min;
    u64 power_cycles;
    u64 power_on_hours;
    u64 unsafe_shutdowns;
    u64 media_errors;
    u64 error_log_entries;

    u32 warning_temp_time_min;
    u32 critical_temp_time_min;
} SmartNvme;

typedef struct SmartAttribute {
    u8  id;
    u16 flags;
    u8  current;
    u8  worst;
    u8  threshold;
    u8  raw[6];
} __attribute__((packed)) SmartAttribute;

typedef struct SmartAta {
    u16            version;
    u8             attr_count;
    SmartAttribute attrs[30];

    u8  temperature_celsius;
    u64 power_on_hours;
    u32 power_cycles;
    u32 reallocated_sectors;
    u32 pending_sectors;
    u32 uncorrectable_sectors;
    u8  health_ok;
} SmartAta;

typedef struct SmartRawData {
    u8 data[512];
} SmartRawData;

#endif