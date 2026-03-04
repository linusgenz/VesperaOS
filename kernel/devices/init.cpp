// init.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 15.11.25.
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

#include "../../filesystem/devfs/devfs.h"
#include "log_device.h"
#include "misc/cpuinfo.h"
#include "misc/full.h"
#include "misc/meminfo.h"
#include "misc/null.h"
#include "misc/rtc.h"
#include "misc/uptime.h"
#include "misc/urandom.h"
#include "misc/version.h"
#include "misc/zero.h"

void initialize_pseudo_devices() {
    constexpr size_t CHANNEL_CAPACITY = static_cast<size_t>(32) * 1024;
    Channel* kernel_log_channel = Channel::create(CHANNEL_CAPACITY);

    auto* zero_dev = new ZeroDevice("zero");
    auto* null_dev = new NullDevice("null");
    auto* urand_dev = new URandomDevice("urandom");
    auto* full_dev = new FullDevice("full");
    auto* rtc_dev = new RTCDevice("rtc");
    auto* uptime_dev = new UptimeDevice("uptime");
    auto* version_dev = new VersionDevice("version");
    auto* cpuinfo_dev = new CPUInfoDevice("cpuinfo");
    auto* meminfo_dev = new MemInfoDevice("meminfo");
    auto* log_dev = new LogDevice(kernel_log_channel);

    auto register_char_device = [](CharDevice* dev, const char* name, DeviceClass cls) -> KernelDevice* {
        KernelDevice* kd =
            DeviceManager::RegisterCharDevice(dev, name, cls, BusType::BUS_NONE, ControllerType::None, nullptr);
        if (kd) DevFS::register_device(kd);
        return kd;
    };

    register_char_device(zero_dev, "zero", DeviceClass::Pseudo);
    register_char_device(null_dev, "null", DeviceClass::Pseudo);
    register_char_device(urand_dev, "urandom", DeviceClass::Pseudo);
    register_char_device(full_dev, "full", DeviceClass::Pseudo);
    register_char_device(rtc_dev, "rtc", DeviceClass::Misc);
    register_char_device(uptime_dev, "uptime", DeviceClass::Pseudo);
    register_char_device(version_dev, "version", DeviceClass::Pseudo);
    register_char_device(cpuinfo_dev, "cpuinfo", DeviceClass::Pseudo);
    register_char_device(meminfo_dev, "meminfo", DeviceClass::Pseudo);
    register_char_device(log_dev, "log", DeviceClass::Pseudo);
}
