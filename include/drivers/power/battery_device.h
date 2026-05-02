// battery_device.h
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
#ifndef VESPERAOS_BATTERY_DEVICE_H
#define VESPERAOS_BATTERY_DEVICE_H

#include <acpi/acpi.h>
#include <uapi/vespera/dev/power.h>
#include <vespera/devices/char_device.h>
#include <vespera/types.h>

class BatteryDevice final : public CharDevice {
   public:
    BatteryDevice(kernel::acpi::acpi_handle_t handle, u32 index);
    ~BatteryDevice() override;

    BatteryDevice(const BatteryDevice&) = delete;
    BatteryDevice& operator=(const BatteryDevice&) = delete;

    void install_notify_handler();

    // Called by power_driver when AC state changes — triggers a status refresh.
    void on_notify(u32 event);

    // CharDevice interface
    int open(CharFile**) override;
    int release(CharFile*) override;
    isize read(CharFile*, void* buffer, usize count, usize offset) override;
    isize write(CharFile*, const void*, usize) override;
    int ioctl(CharFile*, u32 request, void* arg) override;

    // Accessors backed by cached _BIX/_BIF data.
    bool get_model(char* out, usize len);
    bool get_serial(char* out, usize len);
    bool get_vendor(char* out, usize len);

   private:
    [[nodiscard]] bool query_status(battery_status& out) const;
    [[nodiscard]] bool query_info(battery_info& out) const;
    [[nodiscard]] bool ensure_info();

    static void notify_dispatch(kernel::acpi::acpi_handle_t device, u32 event, void* context);

    kernel::acpi::acpi_handle_t handle_;
    u32 index_;

    bool info_valid_ = false;
    battery_info cached_info_{};
};

#endif  // VESPERAOS_BATTERY_DEVICE_H
