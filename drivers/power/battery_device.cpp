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
#include <uapi/vespera/dev/power.h>
#include <vespera/ipc/vbus_manager.h>
#include <vespera/log.h>
#include <vespera_errno.h>

#include "uapi/vespera/vbus.h"

BatteryDevice::BatteryDevice(kernel::acpi::acpi_handle_t handle, u32 index)
    : CharDevice(BusType::None)
    , handle_(handle)
    , index_(index) {
}

BatteryDevice::~BatteryDevice() {
    kernel::acpi::remove_notify(handle_, kernel::acpi::notify_type::device, notify_dispatch);
}

void BatteryDevice::notify_dispatch(kernel::acpi::acpi_handle_t /*device*/, u32 event, void* context) {
    static_cast<BatteryDevice*>(context)->on_notify(event);
}

void BatteryDevice::on_notify(u32 event) {
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

    vbus_battery_t batt{};
    batt.percent = st.percent;
    batt.present = st.present;
    batt.charging = (st.state & 0x02) ? 1 : 0;
    batt.critical = (st.state & 0x04) ? 1 : 0;
    batt.remaining_mwh = st.remaining_capacity;
    batt.rate_mw = st.present_rate;
    batt.index = static_cast<uint8_t>(index_);

    battery_info info{};
    if (query_info(info)) {
        batt.full_capacity_mwh = info.last_full_capacity;
    }

    VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_BATTERY_CHANGED, &batt, sizeof(batt));
}

void BatteryDevice::install_notify_handler() {
    if (!kernel::acpi::install_notify(handle_, kernel::acpi::notify_type::device, notify_dispatch, this)) {
        Log::warning("[bat%u] install_notify_handler failed", index_);
    }
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
            return query_status(*static_cast<battery_status*>(arg)) ? 0 : -EIO;
        }
        case IOCTL_BAT_GET_INFO: {
            if (!arg) return -EINVAL;
            return query_info(*static_cast<battery_info*>(arg)) ? 0 : -EIO;
        }
        default:
            return -ENOTTY;
    }
}

bool BatteryDevice::query_status(battery_status& out) const {
    if (!kernel::acpi::battery_present(handle_)) {
        out = {};
        out.percent = 255;  // unknown — battery removed
        return true;
    }

    kernel::acpi::bst_data bst{};
    if (!kernel::acpi::query_bst(handle_, bst)) {
        Log::error("battery%u: _BST query failed", index_);
        return false;
    }

    out.state = bst.state;
    out.present_rate = bst.present_rate;
    out.remaining_capacity = bst.remaining_capacity;
    out.present_voltage = bst.present_voltage;
    out.present = 1;

    // Compute percentage from _BIF last-full-capacity.
    kernel::acpi::bif_data bif{};
    if (kernel::acpi::query_bif(handle_, bif)) {
        const u32 full = bif.last_full_capacity;
        if (full != 0xFFFFFFFF && full > 0 && bst.remaining_capacity != 0xFFFFFFFF) {
            const u32 pct = (bst.remaining_capacity * 100u) / full;
            out.percent = static_cast<u8>(pct > 100 ? 100 : pct);
        } else {
            out.percent = 255;
        }
    } else {
        out.percent = 255;
    }

    return true;
}

bool BatteryDevice::query_info(battery_info& out) const {
    kernel::acpi::bif_data bif{};
    if (!kernel::acpi::query_bif(handle_, bif)) {
        Log::error("battery%u: _BIX/_BIF query failed", index_);
        return false;
    }

    out = {};
    out.design_capacity = bif.design_capacity;
    out.last_full_capacity = bif.last_full_capacity;
    out.design_voltage = bif.design_voltage;
    out.capacity_warning = bif.capacity_warning;
    strncpy(out.model, bif.model, sizeof(out.model) - 1);
    strncpy(out.serial, bif.serial, sizeof(out.serial) - 1);
    strncpy(out.type, bif.type, sizeof(out.type) - 1);
    strncpy(out.oem, bif.oem, sizeof(out.oem) - 1);
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
