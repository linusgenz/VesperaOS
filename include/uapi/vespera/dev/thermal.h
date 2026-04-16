// thermal.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 15.04.26.
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

#ifndef VESPERAOS_UAPI_VESPERA_DEV_THERMAL_H
#define VESPERAOS_UAPI_VESPERA_DEV_THERMAL_H

#include <vespera/types.h>

/// Maximum number of thermal zones returned in a single read().
#define THERMAL_MAX_ZONES 8u

/// Source of a thermal measurement.
typedef enum thermal_source : u32 {
    THERMAL_SOURCE_MSR = 0,     ///< Intel Package Thermal MSR (IA32_PACKAGE_THERM_STATUS)
    THERMAL_SOURCE_ACPI = 1,    ///< ACPI ThermalZone _TMP method
} thermal_source_t;

/// Per-zone thermal snapshot.
typedef struct thermal_zone {
    char name[16];            ///< Human-readable zone name, e.g. "pkg0", "tz0" (null-terminated)
    u32 temp_mc;              ///< Current temperature in milli-Celsius
    u32 crit_mc;              ///< Critical trip-point in milli-Celsius (0 = not available)
    thermal_source_t source;  ///< How this value was obtained
    u32 _pad;                 ///< Explicit padding — reserved, must be zero
} thermal_zone_t;

/// Aggregate thermal snapshot for all detected zones.
typedef struct thermal_info {
    u32 zone_count;           ///< Number of valid entries in zones[]
    thermal_zone_t zones[THERMAL_MAX_ZONES];  ///< Per-zone data (up to THERMAL_MAX_ZONES)
} thermal_info_t;

#endif  // VESPERAOS_UAPI_VESPERA_DEV_THERMAL_H