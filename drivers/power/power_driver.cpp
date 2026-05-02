// power_driver.cpp
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
#include <drivers/power/power_driver.h>
#include <klib/string.h>
#include <uapi/vespera/dev/power.h>
#include <vespera/devices/char_device.h>
#include <vespera/devices/device_manager.h>
#include <vespera/filesystem/devfs.h>
#include <vespera/ipc/vbus_manager.h>
#include <vespera/log.h>
#include <vespera_errno.h>

#include "uapi/vespera/vbus.h"

namespace power {
    struct ac_notify_ctx {
        BatteryDevice** batteries;
        u32 count;
    };
    static ac_notify_ctx s_ac_ctx;

    class PowerDevice final : public CharDevice {
       public:
        explicit PowerDevice(u32 battery_count)
            : CharDevice(BusType::None)
            , battery_count_(battery_count) {
        }

        int open(CharFile**) override {
            return 0;
        }
        int release(CharFile*) override {
            return 0;
        }
        isize read(CharFile*, void*, usize, usize) override {
            return -EUNSUPPORTED;
        }
        isize write(CharFile*, const void*, usize) override {
            return -EUNSUPPORTED;
        }

        int ioctl(CharFile*, const u32 request, void* arg) override {
            switch (request) {
                case IOCTL_POWER_SHUTDOWN:
                    Log::info("power: shutdown requested via ioctl");
                    kernel::acpi::power_off();

                case IOCTL_POWER_REBOOT:
                    Log::info("power: reboot requested via ioctl");
                    kernel::acpi::reboot();

                case IOCTL_POWER_GET_COUNT:
                    if (!arg) return -EINVAL;
                    *static_cast<u32*>(arg) = battery_count_;
                    return 0;

                default:
                    return -ENOTTY;
            }
        }

       private:
        u32 battery_count_;
    };

    static u32 s_battery_count = 0;
    static BatteryDevice* s_batteries[8] = {};

    static void ac_notify(kernel::acpi::acpi_handle_t device, u32 event, void* context) {
        if (event != 0x80) return;  // 0x80 = AC status changed

        const kernel::acpi::eval_result psr = kernel::acpi::evaluate_integer(device, "_PSR");
        vbus_ac_t ac{};
        if (psr.ok) {
            ac.online = psr.integer ? 1 : 0;
        }
        VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_AC_CHANGED, &ac, sizeof(ac));

        // Trigger a battery status refresh so consumers get updated SOC.
        auto* ctx = static_cast<ac_notify_ctx*>(context);
        for (u32 i = 0; i < ctx->count; i++) {
            if (ctx->batteries[i]) {
                ctx->batteries[i]->on_notify(0x80);
            }
        }
    }

    static void lid_notify(kernel::acpi::acpi_handle_t device, u32 event, void* /*context*/) {
        if (event != 0x80) return;

        const kernel::acpi::eval_result lid_val = kernel::acpi::evaluate_integer(device, "_LID");
        vbus_lid_t lid{};
        if (lid_val.ok) {
            lid.open = lid_val.integer ? 1 : 0;
        }
        VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_LID_CHANGED, &lid, sizeof(lid));
    }

    struct battery_enum_ctx {
        u32 index;
        BatteryDevice* devices[8];
    };

    static bool on_battery_found(kernel::acpi::acpi_handle_t device, void* context) {
        auto* ctx = static_cast<battery_enum_ctx*>(context);
        if (ctx->index >= 8) return false;

        char name[8];
        snprintf(name, sizeof(name), "bat%u", ctx->index);

        auto* bat = new BatteryDevice(device, ctx->index);

        KernelDevice* kd = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name(name)
                .set_type(DeviceType::Char)
                .set_class(DeviceClass::Misc)
                .set_bus(BusType::None)
                .with_char(bat)
        );

        if (kd) {
            DevFs::register_device(kd);
            Log::ok("power: registered /dev/%s", name);
            ctx->devices[ctx->index] = bat;
            s_batteries[ctx->index] = bat;
        } else {
            Log::error("power: failed to register /dev/%s", name);
            delete bat;
        }

        ctx->index++;
        return true;
    }

    static bool on_ac_found(kernel::acpi::acpi_handle_t device, void* context) {
        auto* ctx = static_cast<ac_notify_ctx*>(context);
        if (!kernel::acpi::install_notify(device, kernel::acpi::notify_type::all, ac_notify, ctx)) {
            Log::warning("power: AC notify install failed");
        }
        return true;
    }

    static bool on_lid_found(kernel::acpi::acpi_handle_t device, void* /*context*/) {
        kernel::acpi::install_notify(device, kernel::acpi::notify_type::all, lid_notify, nullptr);
        return true;
    }

    void init() {
        battery_enum_ctx bat_ctx{.index = 0};
        kernel::acpi::enumerate_devices("PNP0C0A", on_battery_found, &bat_ctx);
        s_battery_count = bat_ctx.index;

        if (s_battery_count == 0) {
            Log::info("power: no ACPI batteries found (desktop or VM?)");
        } else {
            Log::ok("power: found %u batter%s", s_battery_count, s_battery_count == 1 ? "y" : "ies");
            for (u32 i = 0; i < s_battery_count; i++) {
                if (bat_ctx.devices[i]) {
                    bat_ctx.devices[i]->install_notify_handler();
                }
            }
        }

        // AC adapters — pass battery array so the AC notify can refresh SOC.
        s_ac_ctx.batteries = s_batteries;
        s_ac_ctx.count     = s_battery_count;
        kernel::acpi::enumerate_devices("ACPI0003", on_ac_found, &s_ac_ctx);

        // Lid
        kernel::acpi::enumerate_devices("PNP0C0D", on_lid_found, nullptr);

        // /dev/power
        auto* pwr = new PowerDevice(s_battery_count);
        KernelDevice* kd = DeviceManager::register_device(
            DeviceDescriptor{}
                .set_name("power")
                .set_type(DeviceType::Char)
                .set_class(DeviceClass::Misc)
                .set_bus(BusType::None)
                .with_char(pwr)
        );

        if (kd) {
            DevFs::register_device(kd);
            Log::ok("power: registered /dev/power");
        } else {
            Log::error("power: failed to register /dev/power");
            delete pwr;
        }
    }

    u32 get_battery_count() {
        return s_battery_count;
    }

}  // namespace power
