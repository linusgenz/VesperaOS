// thermal.cpp
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

#include "thermal.h"

#include <acpi/acpi.h>
#include <arch/x86_64/cpu/msr.h>
#include <klib/string.h>
#include <uapi/vespera/dev/thermal.h>

// Reference: Volume 3B: System Programming Guide, Part 2 (14.8)

/// Intel MSR: Package Thermal Status.
constexpr u32 MSR_PACKAGE_THERM_STATUS = 0x1B1u;

/// Intel MSR: Temperature Target.
constexpr u32 MSR_TEMPERATURE_TARGET = 0x1A2u;

namespace {

    // Returns true if the CPU advertises package thermal MSR support via
    // CPUID.06H:EAX[6].
    bool cpu_has_package_thermal() {
        u32 eax = 0;
        asm volatile("cpuid" : "=a"(eax) : "a"(0x06u) : "ebx", "ecx", "edx");
        return (eax & (1u << 6)) != 0;
    }

    u32 read_tjmax_celsius() {
        const u64 val = rdmsr(MSR_TEMPERATURE_TARGET);
        const u32 tjmax = static_cast<u32>((val >> 16) & 0xFFu);
        return (tjmax != 0) ? tjmax : 100u;
    }

    bool read_package_thermal_msr(thermal_zone_t* zone) {
        if (!cpu_has_package_thermal()) return false;

        const u64 status = rdmsr(MSR_PACKAGE_THERM_STATUS);
        if (!(status & (1ull << 31))) return false;  // reading not valid

        const u32 readout = static_cast<u32>((status >> 16) & 0x7Fu);
        const u32 tjmax = read_tjmax_celsius();
        const u32 temp_c = (readout <= tjmax) ? (tjmax - readout) : tjmax;

        memset(zone->name, 0, sizeof(zone->name));
        memcpy(zone->name, "pkg0", 4);

        zone->temp_mc = temp_c * 1000u;
        zone->crit_mc = tjmax * 1000u;
        zone->source = THERMAL_SOURCE_MSR;
        zone->_pad = 0;

        return true;
    }

    // ─── ACPI thermal zone enumeration ───────────────────────────────────────

    struct tz_walk_ctx {
        thermal_info_t* info;
        u32 max_zones;
    };

    static bool acpi_tz_walk(kernel::acpi::acpi_handle_t object, u32 /*nesting*/, void* context) {
        auto* ctx = static_cast<tz_walk_ctx*>(context);
        thermal_info_t* info = ctx->info;

        if (info->zone_count >= ctx->max_zones) return false;

        // _TMP: temperature in tenths of Kelvin.
        const kernel::acpi::eval_result tmp = kernel::acpi::evaluate_integer(object, "_TMP");
        if (!tmp.ok) return true;  // skip this zone, continue walk

        const u64 tenths_k = tmp.integer;
        if (tenths_k < 2732u) return true;  // below 0 °C — spurious, skip

        const u64 temp_mc = (tenths_k - 2732u) * 100u;

        // _CRT: critical trip point (optional).
        u32 crit_mc = 0;
        const kernel::acpi::eval_result crt = kernel::acpi::evaluate_integer(object, "_CRT");
        if (crt.ok && crt.integer >= 2732u) {
            crit_mc = static_cast<u32>((crt.integer - 2732u) * 100u);
        }

        thermal_zone_t* zone = &info->zones[info->zone_count];
        memset(zone->name, 0, sizeof(zone->name));

        if (!kernel::acpi::get_object_name(object, zone->name, sizeof(zone->name))) {
            // Fallback: tzN
            zone->name[0] = 't';
            zone->name[1] = 'z';
            zone->name[2] = static_cast<char>('0' + info->zone_count);
            zone->name[3] = '\0';
        }

        zone->temp_mc = static_cast<u32>(temp_mc);
        zone->crit_mc = crit_mc;
        zone->source = THERMAL_SOURCE_ACPI;
        zone->_pad = 0;

        info->zone_count++;
        return true;
    }

    void enumerate_acpi_thermal_zones(thermal_info_t* info, u32 max_zones) {
        tz_walk_ctx ctx{info, max_zones};
        kernel::acpi::walk_namespace(kernel::acpi::acpi_object_type::thermal, acpi_tz_walk, &ctx);
    }

}  // namespace

ThermalDevice::ThermalDevice()
    : CharDevice(BusType::VIRTUAL) {
}

int ThermalDevice::open(CharFile**) {
    return 0;
}

int ThermalDevice::release(CharFile*) {
    return 0;
}

isize ThermalDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer || count < sizeof(thermal_info_t)) return -EINVAL;

    thermal_info_t info{};
    info.zone_count = 0;

    if (read_package_thermal_msr(&info.zones[0])) {
        info.zone_count = 1;
    }

    constexpr u32 MAX_ZONES = 8u;
    enumerate_acpi_thermal_zones(&info, MAX_ZONES);

    memcpy(buffer, &info, sizeof(thermal_info_t));
    return sizeof(thermal_info_t);
}

isize ThermalDevice::write(CharFile*, const void*, const usize) {
    return -EUNSUPPORTED;
}
