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
#include <vespera/devices/device_info.h>

class BatteryDevice final : public CharDevice, public IDeviceInfo {
   public:
    explicit BatteryDevice(ACPI_HANDLE handle, u32 index);
    ~BatteryDevice() override;
    static void notify_handler_trampoline(ACPI_HANDLE, UINT32 event, void* context);
    void on_notify(UINT32 event);
    void install_notify_handler();
    void remove_notify_handler() const;

    int open(CharFile** out_cf) override;
    int release(CharFile* cf) override;
    isize read(CharFile* cf, void* buffer, usize count, usize offset) override;
    isize write(CharFile* cf, const void* buffer, usize count) override;
    int ioctl(CharFile* cf, u32 request, void* arg) override;

    bool get_model(char* out, usize len) override;
    bool get_serial(char* out, usize len) override;
    bool get_vendor(char* out, usize len) override;

   private:
    bool query_status(battery_status& out) const;
    bool query_info(battery_info& out) const;
    bool ensure_info();

    ACPI_HANDLE handle_;
    u32 index_;
    bool info_valid_ = false;
    battery_info cached_info_;
};

#endif  // VESPERAOS_BATTERY_DEVICE_H
