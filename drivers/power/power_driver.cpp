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
#include <vespera/log.h>
#include <vespera_errno.h>

#include "uapi/vespera/vbus.h"
#include "vespera/ipc/vbus_manager.h"

namespace power {
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
                    acpi::acpi_power_off();
                    return 0;

                case IOCTL_POWER_REBOOT:
                    Log::info("power: reboot requested via ioctl");
                    acpi::acpi_reboot();
                    return 0;

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

    struct BatteryContext {
        u32 index;
        BatteryDevice* devices[8];
    };

    static ACPI_STATUS on_battery_found(
        ACPI_HANDLE object, u32 /* nesting_level */, void* context, void** /* return_value */
    ) {
        auto* ctx = static_cast<BatteryContext*>(context);

        // Build device name: bat0, bat1, …
        char name[8];
        snprintf(name, sizeof(name), "bat%u", ctx->index);

        auto* bat = new BatteryDevice(object, ctx->index);

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
        return AE_OK;
    }

    static void ac_notify_handler(ACPI_HANDLE device, UINT32 event, void* /*context*/) {
        if (event != 0x80) return;  // 0x80 = AC status changed

        ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
        vbus_ac_t ac{};

        if (ACPI_SUCCESS(AcpiEvaluateObject(device, "_PSR", nullptr, &result)) && result.Pointer) {
            const auto* obj = static_cast<ACPI_OBJECT*>(result.Pointer);
            if (obj->Type == ACPI_TYPE_INTEGER) {
                ac.online = obj->Integer.Value ? 1 : 0;
            }
            AcpiOsFree(result.Pointer);
        }

      //  Log::debug("[AC] adapter %s", ac.online ? "online" : "offline");
        VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_AC_CHANGED, &ac, sizeof(ac));

        for (u32 i = 0; i < s_battery_count; i++) {
            if (s_batteries[i]) {
                s_batteries[i]->on_notify(0x80);
            }
        }
    }

    struct AcContext {
        u32 count;
        ACPI_HANDLE handles[4];
    };

    static ACPI_STATUS on_ac_found(ACPI_HANDLE object, u32, void* context, void**) {
        auto* ctx = static_cast<AcContext*>(context);
        if (ctx->count < 4) ctx->handles[ctx->count] = object;
        ctx->count++;
        return AE_OK;
    }

    static void lid_notify_handler(ACPI_HANDLE device, UINT32 event, void*) {
        if (event != 0x80) return;

        ACPI_BUFFER result = {ACPI_ALLOCATE_BUFFER, nullptr};
        vbus_lid_t lid{};

        if (ACPI_SUCCESS(AcpiEvaluateObject(device, "_LID", nullptr, &result)) && result.Pointer) {
            const auto* obj = static_cast<ACPI_OBJECT*>(result.Pointer);
            if (obj->Type == ACPI_TYPE_INTEGER) {
                lid.open = obj->Integer.Value ? 1 : 0;
            }
            AcpiOsFree(result.Pointer);
        }

        //Log::debug("[LID] %s", lid.open ? "opened" : "closed");
        VBusManager::emit(VBUS_IFACE_POWER, VBUS_SIG_LID_CHANGED, &lid, sizeof(lid));
    }

    static ACPI_STATUS on_lid_found(ACPI_HANDLE object, u32, void*, void**) {
        AcpiInstallNotifyHandler(object, ACPI_ALL_NOTIFY, lid_notify_handler, nullptr);
        return AE_OK;
    }

    void init() {
        BatteryContext ctx{.index = 0};
        AcpiGetDevices(const_cast<char*>("PNP0C0A"), on_battery_found, &ctx, nullptr);
        s_battery_count = ctx.index;

        if (s_battery_count == 0) {
            Log::info("power: no ACPI batteries found (desktop or VM?)");
        } else {
            Log::ok("power: found %u batter%s", s_battery_count, s_battery_count == 1 ? "y" : "ies");

            for (u32 i = 0; i < s_battery_count; i++) {
                if (ctx.devices[i]) {
                    ctx.devices[i]->install_notify_handler();
                }
            }
        }

        // Power supply (connected or not)
        AcContext ac_ctx{.count = 0};
        AcpiGetDevices(const_cast<char*>("ACPI0003"), on_ac_found, &ac_ctx, nullptr);

        for (u32 i = 0; i < ac_ctx.count; i++) {
            const ACPI_STATUS st =
                AcpiInstallNotifyHandler(ac_ctx.handles[i], ACPI_ALL_NOTIFY, ac_notify_handler, nullptr);
            if (ACPI_FAILURE(st)) {
                Log::warning("power: AC notify install failed: %u", st);
            }
        }

        // Laptop lid
        AcpiGetDevices(const_cast<char*>("PNP0C0D"), on_lid_found, nullptr, nullptr);

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
