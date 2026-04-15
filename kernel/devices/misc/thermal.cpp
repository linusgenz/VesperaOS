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

#include <arch/x86_64/cpu/msr.h>
#include <klib/string.h>
#include <uapi/vespera/dev/thermal.h>

#include <acpi/acpi.h>


/// Intel MSR: Package Thermal Status.
/// Bits 22:16 — Package Digital Readout (degrees below TCC activation).
/// Bit 31 — Reading valid flag (1 = valid).
constexpr u32 MSR_PACKAGE_THERM_STATUS = 0x1B1u;

/// Intel MSR: Temperature Target.
/// Bits 23:16 — TJMAX (TCC activation temperature in degrees Celsius).
constexpr u32 MSR_TEMPERATURE_TARGET = 0x1A2u;

namespace {

    /// Returns true if the CPU advertises digital thermal sensor support
    /// via CPUID.06H:EAX[0] (per-core DTS) and package thermal MSR support
    /// via CPUID.06H:EAX[6].
    bool cpu_has_package_thermal() {
        u32 eax = 0;
        asm volatile("cpuid" : "=a"(eax) : "a"(0x06u) : "ebx", "ecx", "edx");
        return (eax & (1 << 6));
    }

    /// Reads TJMAX from MSR_TEMPERATURE_TARGET bits 23:16.
    /// Returns 100 °C as a safe fallback if the MSR read yields zero.
    u32 read_tjmax_celsius() {
        u64 val = rdmsr(MSR_TEMPERATURE_TARGET);
        u32 tjmax = static_cast<u32>((val >> 16) & 0xFFu);
        return (tjmax != 0) ? tjmax : 100u;
    }

    bool read_package_thermal_msr(thermal_zone_t* zone) {
        if (!cpu_has_package_thermal()) {
            return false;
        }

        u64 status = rdmsr(MSR_PACKAGE_THERM_STATUS);

        // Bit 31: reading valid
        if (!(status & (1ull << 31))) {
            return false;
        }

        // Bits 22:16: degrees below TJMAX (higher readout = cooler)
        u32 readout = static_cast<u32>((status >> 16) & 0x7Fu);
        u32 tjmax = read_tjmax_celsius();
        u32 temp_c = (readout <= tjmax) ? (tjmax - readout) : tjmax;

        memset(zone->name, 0, sizeof(zone->name));
        memcpy(zone->name, "pkg0", 4);

        zone->temp_mc = temp_c * 1000u;
        zone->crit_mc = tjmax * 1000u;
        zone->source = THERMAL_SOURCE_MSR;
        zone->_pad = 0;

        return true;
    }

    struct acpi_tz_ctx {
        thermal_info_t* info;
        u32 max_zones;
    };

    /// ACPI namespace walk callback — called once per ACPI object.
    /// Filters for ThermalZone objects (type ACPI_TYPE_THERMAL) and evaluates
    /// their _TMP method to obtain the current temperature.
    ACPI_STATUS
    acpi_thermal_walk_cb(ACPI_HANDLE object, UINT32 /*nesting_level*/, void* context, void** /*return_value*/) {
        auto* ctx = static_cast<acpi_tz_ctx*>(context);
        thermal_info_t* info = ctx->info;

        if (info->zone_count >= ctx->max_zones) {
            return AE_OK;
        }

        // temperature in tenths of Kelvin
        ACPI_BUFFER result{};
        result.Length = ACPI_ALLOCATE_BUFFER;
        result.Pointer = nullptr;

        ACPI_STATUS status =
            AcpiEvaluateObjectTyped(object, const_cast<ACPI_STRING>("_TMP"), nullptr, &result, ACPI_TYPE_INTEGER);

        if (ACPI_FAILURE(status)) {
            return AE_OK;
        }

        auto* obj = static_cast<ACPI_OBJECT*>(result.Pointer);
        // ACPI temperature: tenths of Kelvin.  Convert to milli-Celsius:
        //   T_mc = (T_tenths_K - 2732) * 100
        u64 tenths_k = obj->Integer.Value;
        AcpiOsFree(result.Pointer);

        if (tenths_k < 2732u) {
            // idk how this should happen but we catch this anyway
            return AE_OK;
        }

        u64 temp_mc = (tenths_k - 2732u) * 100u;

        // Retrieve _CRT (critical trip point) if available; ignore on failure.
        u32 crit_mc = 0;
        ACPI_BUFFER crit_buf{};
        crit_buf.Length = ACPI_ALLOCATE_BUFFER;
        crit_buf.Pointer = nullptr;

        if (ACPI_SUCCESS(
                AcpiEvaluateObjectTyped(object, const_cast<ACPI_STRING>("_CRT"), nullptr, &crit_buf, ACPI_TYPE_INTEGER)
            )) {
            auto* cobj = static_cast<ACPI_OBJECT*>(crit_buf.Pointer);
            if (cobj->Integer.Value >= 2732u) {
                crit_mc = static_cast<u32>((cobj->Integer.Value - 2732u) * 100u);
            }
            AcpiOsFree(crit_buf.Pointer);
        }

        // Build a zone name from the ACPI object path (last 4 chars).
        thermal_zone_t* zone = &info->zones[info->zone_count];
        memset(zone->name, 0, sizeof(zone->name));

        ACPI_BUFFER name_buf{};
        name_buf.Length = ACPI_ALLOCATE_BUFFER;
        name_buf.Pointer = nullptr;
        if (ACPI_SUCCESS(AcpiGetName(object, ACPI_SINGLE_NAME, &name_buf))) {
            const char* src = static_cast<const char*>(name_buf.Pointer);
            usize len = strlen(src);
            usize copy = (len < 15u) ? len : 15u;
            memcpy(zone->name, src, copy);
            AcpiOsFree(name_buf.Pointer);
        } else {
            // Fallback name
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
        return AE_OK;
    }

    void enumerate_acpi_thermal_zones(thermal_info_t* info, u32 max_zones) {
        acpi_tz_ctx ctx{info, max_zones};
        AcpiWalkNamespace(
            ACPI_TYPE_THERMAL, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX, acpi_thermal_walk_cb, nullptr, &ctx, nullptr
        );
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
    if (!buffer || count < sizeof(thermal_info_t)) {
        return -EINVAL;
    }

    thermal_info_t info{};
    info.zone_count = 0;

    // Zone 0 is always Intel Package Thermal MSR (if available)
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