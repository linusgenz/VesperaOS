// battery_device.cpp
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

#include <acpi/acpi.h>
#include <drivers/power/battery_device.h>
#include <klib/string.h>
#include <vespera/log.h>
#include <vespera_errno.h>

#include "uapi/vespera/vbus.h"
#include "vespera/ipc/vbus_manager.h"

static void copy_acpi_string(const ACPI_OBJECT& obj, char* dst, usize dst_size) {
    if (obj.Type == ACPI_TYPE_STRING && obj.String.Pointer) {
        const usize len = obj.String.Length < dst_size - 1 ? obj.String.Length : dst_size - 1;
        memcpy(dst, obj.String.Pointer, len);
        dst[len] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static u32 acpi_int(const ACPI_OBJECT& obj) {
    return obj.Type == ACPI_TYPE_INTEGER ? static_cast<u32>(obj.Integer.Value) : 0xFFFFFFFFu;
}

BatteryDevice::BatteryDevice(ACPI_HANDLE handle, u32 index)
    : CharDevice(BusType::None)
    , handle_(handle)
    , index_(index) {
}

BatteryDevice::~BatteryDevice() {
    remove_notify_handler();
}

void BatteryDevice::notify_handler_trampoline(ACPI_HANDLE /*device*/, UINT32 event, void* context) {
    auto* self = static_cast<BatteryDevice*>(context);
    self->on_notify(event);
}

void BatteryDevice::on_notify(UINT32 event) {
    // ACPI Spec §10.2.2.6:
    //   0x80 = Battery Status Changed  (_BST has new data)
    //   0x81 = Battery Info Changed    (_BIF/_BIX has new data – capacity changed)
    if (event != 0x80 && event != 0x81) return;
    if (event == 0x81) {
        info_valid_ = false;
    }

    battery_status st{};
    if (!query_status(st)) {
        Log::warning("[bat%u] notify: query_status failed", index_);
        return;
    }

    // Convert battery_status → vbus_battery_t
    vbus_battery_t batt{};
    batt.percent = st.percent;
    batt.present = st.present;
    batt.charging = (st.state & 0x02) ? 1 : 0;
    batt.critical = (st.state & 0x04) ? 1 : 0;
    batt.remaining_mwh = st.remaining_capacity;
    batt.rate_mw = st.present_rate;
    batt.index = static_cast<uint8_t>(index_);

    // Fetch last-full capacity for reference (best-effort)
    battery_info info{};
    if (query_info(info)) {
        batt.full_capacity_mwh = info.last_full_capacity;
    }

    Log::debug(
        "[bat%u] %u%% %s%s",
        index_,
        batt.percent,
        batt.charging ? "charging" : "discharging",
        batt.critical ? " [CRITICAL]" : ""
    );

    VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_BATTERY_CHANGED, &batt, sizeof(batt));
}

void BatteryDevice::install_notify_handler() {
    const ACPI_STATUS st = AcpiInstallNotifyHandler(
        handle_,
        ACPI_DEVICE_NOTIFY,  // device-specific notifications (0x80+)
        notify_handler_trampoline,
        this
    );
    if (ACPI_FAILURE(st)) {
        Log::warning("[bat%u] AcpiInstallNotifyHandler failed: %u", index_, st);
    }
}

void BatteryDevice::remove_notify_handler() const {
    AcpiRemoveNotifyHandler(handle_, ACPI_ALL_NOTIFY, notify_handler_trampoline);
}

int BatteryDevice::open(CharFile**) {
    return 0;
}
int BatteryDevice::release(CharFile*) {
    return 0;
}

isize BatteryDevice::read(CharFile*, void* buffer, const usize count, usize) {
    if (!buffer || count < sizeof(battery_status)) return -EINVAL;

    battery_status status{};
    if (!query_status(status)) return -EIO;

    memcpy(buffer, &status, sizeof(battery_status));
    return sizeof(battery_status);
}

isize BatteryDevice::write(CharFile*, const void*, usize) {
    return -EUNSUPPORTED;
}

int BatteryDevice::ioctl(CharFile*, const u32 request, void* arg) {
    switch (request) {
        case IOCTL_BAT_GET_STATUS: {
            if (!arg) return -EINVAL;
            auto* out = static_cast<battery_status*>(arg);
            return query_status(*out) ? 0 : -EIO;
        }
        case IOCTL_BAT_GET_INFO: {
            if (!arg) return -EINVAL;
            auto* out = static_cast<battery_info*>(arg);
            return query_info(*out) ? 0 : -EIO;
        }
        default:
            return -ENOTTY;
    }
}

bool BatteryDevice::query_status(battery_status& out) const {
    ACPI_BUFFER sta_buf = {ACPI_ALLOCATE_BUFFER, nullptr};
    if (ACPI_SUCCESS(AcpiEvaluateObject(handle_, "_STA", nullptr, &sta_buf)) && sta_buf.Pointer) {
        const auto* obj = static_cast<ACPI_OBJECT*>(sta_buf.Pointer);
        const bool present = (obj->Type == ACPI_TYPE_INTEGER) && (obj->Integer.Value & 0x10);
        AcpiOsFree(sta_buf.Pointer);
        if (!present) {
            out = {};
            out.percent = 255;  // unknown
            return true;        // battery got removed, this is not an error
        }
    }

    ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
    const ACPI_STATUS status = AcpiEvaluateObject(handle_, "_BST", nullptr, &result);
    if (ACPI_FAILURE(status) || !result.Pointer) {
        Log::error("battery%u: _BST evaluation failed: %u", index_, status);
        return false;
    }

    const auto* pkg = static_cast<ACPI_OBJECT*>(result.Pointer);
    if (pkg->Type != ACPI_TYPE_PACKAGE || pkg->Package.Count < 4) {
        AcpiOsFree(result.Pointer);
        Log::error("battery%u: _BST package malformed", index_);
        return false;
    }

    const ACPI_OBJECT* e = pkg->Package.Elements;
    out.state = acpi_int(e[0]);
    out.present_rate = acpi_int(e[1]);
    out.remaining_capacity = acpi_int(e[2]);
    out.present_voltage = acpi_int(e[3]);
    out.present = 1;

    AcpiOsFree(result.Pointer);

    ACPI_BUFFER bif_buf = {ACPI_ALLOCATE_BUFFER, nullptr};
    if (ACPI_SUCCESS(AcpiEvaluateObject(handle_, "_BIF", nullptr, &bif_buf)) && bif_buf.Pointer) {
        const auto* bif = static_cast<ACPI_OBJECT*>(bif_buf.Pointer);
        if (bif->Type == ACPI_TYPE_PACKAGE && bif->Package.Count >= 2) {
            const u32 full = acpi_int(bif->Package.Elements[2]);  // LastFullCapacity
            if (full != 0xFFFFFFFF && full > 0 && out.remaining_capacity != 0xFFFFFFFF) {
                const u32 pct = (out.remaining_capacity * 100u) / full;
                out.percent = static_cast<u8>(pct > 100 ? 100 : pct);
            } else {
                out.percent = 255;
            }
        }
        AcpiOsFree(bif_buf.Pointer);
    } else {
        out.percent = 255;
    }

    return true;
}

bool BatteryDevice::query_info(battery_info& out) const {
    ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
    bool use_bix = ACPI_SUCCESS(AcpiEvaluateObject(handle_, "_BIX", nullptr, &result)) && result.Pointer;

    if (!use_bix) {
        result = {ACPI_ALLOCATE_BUFFER, nullptr};
        if (ACPI_FAILURE(AcpiEvaluateObject(handle_, "_BIF", nullptr, &result)) || !result.Pointer) {
            Log::error("battery%u: neither _BIX nor _BIF available", index_);
            return false;
        }
    }

    const auto* pkg = static_cast<ACPI_OBJECT*>(result.Pointer);
    if (pkg->Type != ACPI_TYPE_PACKAGE) {
        AcpiOsFree(result.Pointer);
        return false;
    }

    const ACPI_OBJECT* e = pkg->Package.Elements;
    const u32 n = pkg->Package.Count;

    out = {};

    if (use_bix) {
        // _BIX layout (ACPI 4.0): element 0 = Revision, 1 = PowerUnit,
        // 2 = DesignCapacity, 3 = LastFullCapacity, 4 = BatteryTechnology,
        // 5 = DesignVoltage, 6 = DesignCapacityWarning, …
        // 12 = ModelNumber, 13 = SerialNumber, 14 = BatteryType, 15 = OEMInfo
        if (n > 2) out.design_capacity = acpi_int(e[2]);
        if (n > 3) out.last_full_capacity = acpi_int(e[3]);
        if (n > 5) out.design_voltage = acpi_int(e[5]);
        if (n > 6) out.capacity_warning = acpi_int(e[6]);
        if (n > 12) copy_acpi_string(e[12], out.model, sizeof(out.model));
        if (n > 13) copy_acpi_string(e[13], out.serial, sizeof(out.serial));
        if (n > 14) copy_acpi_string(e[14], out.type, sizeof(out.type));
        if (n > 15) copy_acpi_string(e[15], out.oem, sizeof(out.oem));
    } else {
        // _BIF layout: 0 = PowerUnit, 1 = DesignCapacity, 2 = LastFullCapacity,
        // 3 = BatteryTechnology, 4 = DesignVoltage, 5 = DesignCapacityWarning,
        // …, 9 = ModelNumber, 10 = SerialNumber, 11 = BatteryType, 12 = OEMInfo
        if (n > 1) out.design_capacity = acpi_int(e[1]);
        if (n > 2) out.last_full_capacity = acpi_int(e[2]);
        if (n > 4) out.design_voltage = acpi_int(e[4]);
        if (n > 5) out.capacity_warning = acpi_int(e[5]);
        if (n > 9) copy_acpi_string(e[9], out.model, sizeof(out.model));
        if (n > 10) copy_acpi_string(e[10], out.serial, sizeof(out.serial));
        if (n > 11) copy_acpi_string(e[11], out.type, sizeof(out.type));
        if (n > 12) copy_acpi_string(e[12], out.oem, sizeof(out.oem));
    }

    AcpiOsFree(result.Pointer);
    return true;
}

bool BatteryDevice::ensure_info() {
    if (info_valid_) return true;
    if (!query_info(cached_info_)) return false;
    info_valid_ = true;
    return true;
}

bool BatteryDevice::get_model(char* out, usize len) {
    if (!ensure_info()) return false;
    strncpy(out, cached_info_.model, len);
    return true;
}

bool BatteryDevice::get_serial(char* out, usize len) {
    if (!ensure_info()) return false;
    strncpy(out, cached_info_.serial, len);
    return true;
}

bool BatteryDevice::get_vendor(char* out, usize len) {
    if (!ensure_info()) return false;

    strncpy(out, cached_info_.oem, len);
    return true;
}