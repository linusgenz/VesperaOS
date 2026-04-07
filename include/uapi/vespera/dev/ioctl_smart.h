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
#include <vespera/ioctl.h>

typedef enum SmartDriverType {
    SMART_DRIVER_UNKNOWN = 0,
    SMART_DRIVER_NVME = 1,
    SMART_DRIVER_ATA = 2,
} SmartDriverType;

typedef struct smart_common {
    SmartDriverType driver_type;
    u8 temperature_celsius;
    u64 power_on_hours;
    u8 health_ok;
    u8 critical_warning_raw;
} smart_common_t;

typedef struct smart_nvme {
    u8 critical_warning_raw;
    u8 available_spare;            // 0-100%
    u8 available_spare_threshold;  // 0-100%
    u8 percentage_used;            // 0-255%
    u8 temperature_celsius;
    u8 temperature_sensor[8];  // Celsius, 0 = not available/existing

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
} smart_nvme_t;

typedef struct smart_attribute {
    u8 id;
    u16 flags;
    u8 current;
    u8 worst;
    u8 threshold;
    u8 raw[6];
} __attribute__((packed)) smart_attribute_t;

typedef struct smart_ata {
    u16 version;
    u8 attr_count;
    smart_attribute_t attrs[30];

    u8 temperature_celsius;
    u64 power_on_hours;
    u32 power_cycles;
    u32 reallocated_sectors;
    u32 pending_sectors;
    u32 uncorrectable_sectors;
    u8 health_ok;
} smart_ata_t;

typedef struct smart_raw {
    u8 data[512];
} smart_raw_t;

#define IOCTL_SMART_GET_RAW    IOR('S', 0x01, smart_raw_t)
#define IOCTL_SMART_GET_COMMON IOR('S', 0x02, smart_common_t)
#define IOCTL_SMART_GET_NVME   IOR('S', 0x03, smart_nvme_t)
#define IOCTL_SMART_GET_ATA    IOR('S', 0x04, smart_ata_t)

#endif